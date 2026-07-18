#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <BLE2902.h>
#include <HardwareSerial.h>
#include <driver/uart.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#define PN532_IRQ   4   // ESP32 的 GPIO4 连接 PN532 的 IRQ 引脚
#define PN532_RESET 5   // ESP32 的 GPIO5 连接 PN532 的 RSTPDN 引脚
#define RELAY_PIN   23  // 控制继电器的 GPIO

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
//Adafruit_PN532 nfc(0x24); // 使用默认 I2C 地址 0x24  

// 预设授权卡 UID（示例：4 字节 UID）
uint8_t authorizedUID[4] = {0x3E, 0x48, 0x0D, 0x02};
bool checkAuthorized(uint8_t *uid, uint8_t uidLength) {
  // 假定授权卡为 4 字节 UID，实际项目中可扩展为多卡判断
  if (uidLength != sizeof(authorizedUID)) return false;
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] != authorizedUID[i]) return false;
  }
  return true;
}
void unlockDoor() {
  digitalWrite(RELAY_PIN, HIGH);  // 打开继电器（解锁）
  delay(5000);                    // 解锁状态保持 5 秒
  digitalWrite(RELAY_PIN, LOW);   // 关闭继电器（上锁）
}

// 数据包结构（使用数组缓冲区）
#pragma pack(push, 1) // 1字节对齐
typedef struct
{
  uint8_t header[2]; // 包头：0xAA, 0xFF
  uint8_t id = 0;
  uint16_t packet_id; // 包序号
  int16_t x;          // X坐标
  int16_t y;          // Y坐标
  int16_t x1;
  int16_t y1;
} RadarPacket_t;
#pragma pack(pop)

#pragma pack(push, 1) // 1字节对齐
typedef struct
{
  uint8_t header[2]; // 包头：0xAA, 0xFF
  uint8_t id;
  uint16_t packet_id; // 包序号
  int16_t x;          // X坐标
  int16_t y;          // Y坐标
} RadarPacket_t1;
#pragma pack(pop)

typedef struct
{
  volatile int16_t m1_xCoordinate;
  volatile int16_t m1_yCoordinate;
  int16_t m1_speed;
  uint16_t m1_DistanceResolution;

  int16_t m2_xCoordinate;
  int16_t m2_yCoordinate;
  int16_t m2_speed;
  uint16_t m2_DistanceResolution;

  int16_t m3_xCoordinate;
  int16_t m3_yCoordinate;
  int16_t m3_speed;
  uint16_t m3_DistanceResolution;
} HiLink2450_t;
uint16_t d1, d2, d3;
HiLink2450_t HiLink_2450;
QueueHandle_t bt_queue; // 蓝牙数据队列
volatile uint8_t Serial_RxFlag = 0;
uint8_t Serial_RxPacket[24];
// 设置目标消失延迟的命令帧
// 硬件串口配置
HardwareSerial RadarSerial(1); // UART1 (RX=16, TX=17)
#define BUF_SIZE 1024
static QueueHandle_t uart_queue;

volatile bool bt_connected = false;
// 定义UART参数
#define UART_NUM UART_NUM_1
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart1_queue;

TaskHandle_t uart1_task_handle;

double calculateDistance(double x, double y)
{
  return sqrt(x * x + y * y);
}
double calculateAngle(double x, double y)
{
  double angle_rad = atan2(x, y);              // 计算反正切（弧度）
  double angle_deg = angle_rad * 180.0 / M_PI; // 弧度转角度
  return angle_deg;
}

void printTargetData()
{
  Serial.println("\n--- 雷达数据 ---");
  if (HiLink_2450.m1_xCoordinate != 0 || HiLink_2450.m1_yCoordinate != 0 || HiLink_2450.m1_speed != 0)
  {
    Serial.printf("目标1: X=%.2fm, Y=%.2fm, 距离=%.2fm, 角度=%.2f°, 速度=%.2fm/s,\n",
                  HiLink_2450.m1_xCoordinate / 1000.0,
                  HiLink_2450.m1_yCoordinate / 1000.0,
                  calculateDistance(HiLink_2450.m1_xCoordinate / 1000.0, HiLink_2450.m1_yCoordinate / 1000.0),
                  calculateAngle(HiLink_2450.m1_xCoordinate, HiLink_2450.m1_yCoordinate),
                  HiLink_2450.m1_speed / 100.0);
  }
  if (HiLink_2450.m2_xCoordinate != 0 || HiLink_2450.m2_yCoordinate != 0 || HiLink_2450.m2_speed != 0)
  {
    Serial.printf("目标2: X=%.2fm, Y=%.2fm, 距离=%.2fmm , 速度=%.2fm/s,\n",
                  HiLink_2450.m2_xCoordinate / 1000.0,
                  HiLink_2450.m2_yCoordinate / 1000.0,
                  HiLink_2450.m2_speed / 100.0,
                  HiLink_2450.m2_DistanceResolution);
  }
  if (HiLink_2450.m3_xCoordinate != 0 || HiLink_2450.m3_yCoordinate != 0 || HiLink_2450.m3_speed != 0)
  {
    Serial.printf("目标3: X=%.2fm, Y=%.2fm, 距离=%.2fmm , 速度=%.2fm/s,\n",
                  HiLink_2450.m3_xCoordinate / 1000.0,
                  HiLink_2450.m3_yCoordinate / 1000.0,
                  HiLink_2450.m3_speed / 100.0,
                  HiLink_2450.m3_DistanceResolution);
  }
  vTaskDelay(10 / portTICK_PERIOD_MS);
}

uint8_t rxBuffer[sizeof(RadarPacket_t1)]; // 接收缓冲区
RadarPacket_t packet;

// double calculateDistance(double x, double y)
// {
//   return sqrt(x * x + y * y);
// }
// double calculateAngle(double x, double y)
// {
//   double angle_rad = atan2(x, y);              // 计算反正切（弧度）
//   double angle_deg = angle_rad * 180.0 / M_PI; // 弧度转角度
//   return angle_deg;
// }
double distance1 = 0;
int door_flag = 0;
// 通用的服务和特征 UUID
#define SERVICE_UUID "5d9f680d-aa9a-43d7-9d88-34049e7663e0"
#define CHARACTERISTIC_UUID "bcf0e6da-9d88-4497-bad3-2fc93acb2f78"
#define SERVICE_UUID1 "a8e4c3b2-5d7e-4f8a-9b0c-3d1e2f4a5b6c"
#define CHARACTERISTIC_UUID1 "d9c8b7a6-5e4f-3d2c-1b0a-9c8d7e6f5a4b"
// 从设备 MAC 地址（你可以替换成第二个从机的实际地址）
const char *slaveMacs[] = {
    "68:25:dd:f0:47:12", // 设备 1
                         //  "68:25:dd:f0:59:32"  // "78:42:1c:6a:fc:7e"  // 从设备 2（示例地址，替换成真实 MAC）
};

BLEServer *pServer = nullptr;
BLECharacteristic *pCharacteristic1 = nullptr;
bool deviceConnected = false;

BLEClient *clients[1] = {nullptr};
BLERemoteCharacteristic *remoteChars[1] = {nullptr};
bool connected[1] = {false};
int16_t x2, y2;
void notifyCallback(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{

  if (length == sizeof(rxBuffer))
  {

    x2 = (pData[6] << 8) | pData[5]; // 大端序转小端序
    y2 = (pData[8] << 8) | pData[7];

    // distance1 = calculateDistance(x2 / 1000.0, y2 / 1000.0);
    //   Serial.printf("车主: X=%.2fm, Y=%.2fm, ID=%d, mmwave=%.2fm\n",
    //                 x2 / 1000.0,
    //                 y2 / 1000.0,
    //                 packet.id,
    //                 distance1);
  }
  else
  {
    Serial.println("收到的数据长度不正确");
  }
}
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("主设备已连接");
  }

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("主设备已断开");
  }
};

bool connectToSlave(int index, BLEAddress address)
{
  clients[index] = BLEDevice::createClient();
  Serial.printf("正在连接从设备 %d...\n", index + 1);
  if (!clients[index]->connect(address))
  {
    Serial.printf("从设备 %d 连接失败\n", index + 1);
    return false;
  }

  Serial.printf("从设备 %d 连接成功\n", index + 1);

  BLERemoteService *service = clients[index]->getService(SERVICE_UUID);
  if (!service)
  {
    Serial.println("未找到服务");
    clients[index]->disconnect();
    return false;
  }

  remoteChars[index] = service->getCharacteristic(CHARACTERISTIC_UUID);
  if (!remoteChars[index])
  {
    Serial.println("未找到特征值");
    clients[index]->disconnect();
    return false;
  }

  if (remoteChars[index]->canNotify())
  {
    remoteChars[index]->registerForNotify(notifyCallback);
    connected[index] = true;
    return true;
  }

  return false;
}

void uart1_task(void *pvParameters)
{
  uart_event_t event;
  uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);

  // 状态机变量（与STM32版本相同）
  uint8_t RxState = 0;
  uint8_t pRxHead[4] = {0};
  uint8_t pRxHeadIndex = 0;
  uint8_t pRxPacketIndex = 0;
  uint8_t checkTailIndex = 0;

  for (;;)
  {
    if (xQueueReceive(uart1_queue, (void *)&event, portMAX_DELAY))
    {
      switch (event.type)
      {
      case UART_DATA:
        uart_read_bytes(UART_NUM, dtmp, event.size, portMAX_DELAY);

        for (int i = 0; i < event.size; i++)
        {
          uint8_t RxData = dtmp[i];

          switch (RxState)
          {
          case 0: // 检查包头
            pRxHead[pRxHeadIndex] = RxData;
            pRxHeadIndex++;
            if (pRxHeadIndex >= 4)
            {
              if (pRxHead[0] == 0xAA && pRxHead[1] == 0xFF &&
                  pRxHead[2] == 0x03 && pRxHead[3] == 0x00)
              {
                RxState = 1;
                pRxHeadIndex = 0;
                pRxPacketIndex = 0;
                checkTailIndex = 0;
              }
              else
              {
                RxState = 0;
                pRxHeadIndex = 0;
              }
            }
            break;

          case 1: // 接收数据
            if (pRxPacketIndex < 24)
            {
              Serial_RxPacket[pRxPacketIndex] = RxData;
              pRxPacketIndex++;

              // 当收到完整数据包时解析
              if (pRxPacketIndex >= 24)
              {
                const int num = 1 << 15;

                /* 解析目标一 */
                uint16_t xRaw1 = Serial_RxPacket[0] + (Serial_RxPacket[1] << 8);
                HiLink_2450.m1_xCoordinate = (Serial_RxPacket[1] & 0x80) ? (xRaw1 - num) : -xRaw1;

                uint16_t yRaw1 = Serial_RxPacket[2] + (Serial_RxPacket[3] << 8);
                HiLink_2450.m1_yCoordinate = yRaw1 - num;

                uint16_t speedRaw1 = Serial_RxPacket[4] + (Serial_RxPacket[5] << 8);
                HiLink_2450.m1_speed = (Serial_RxPacket[5] & 0x80) ? (speedRaw1 - num) : -speedRaw1;

                HiLink_2450.m1_DistanceResolution = Serial_RxPacket[6] + (Serial_RxPacket[7] << 8);

                /* 解析目标二 */
                uint16_t xRaw2 = Serial_RxPacket[8] + (Serial_RxPacket[9] << 8);
                HiLink_2450.m2_xCoordinate = (Serial_RxPacket[9] & 0x80) ? (xRaw2 - num) : -xRaw2;

                uint16_t yRaw2 = Serial_RxPacket[10] + (Serial_RxPacket[11] << 8);
                HiLink_2450.m2_yCoordinate = yRaw2 - num;

                uint16_t speedRaw2 = Serial_RxPacket[12] + (Serial_RxPacket[13] << 8);
                HiLink_2450.m2_speed = (Serial_RxPacket[13] & 0x80) ? (speedRaw2 - num) : -speedRaw2;

                HiLink_2450.m2_DistanceResolution = Serial_RxPacket[14] + (Serial_RxPacket[15] << 8);

                /* 解析目标三 */
                uint16_t xRaw3 = Serial_RxPacket[16] + (Serial_RxPacket[17] << 8);
                HiLink_2450.m3_xCoordinate = (Serial_RxPacket[17] & 0x80) ? (xRaw3 - num) : -xRaw3;

                uint16_t yRaw3 = Serial_RxPacket[18] + (Serial_RxPacket[19] << 8);
                HiLink_2450.m3_yCoordinate = yRaw3 - num;

                uint16_t speedRaw3 = Serial_RxPacket[20] + (Serial_RxPacket[21] << 8);
                HiLink_2450.m3_speed = (Serial_RxPacket[21] & 0x80) ? (speedRaw3 - num) : -speedRaw3;

                HiLink_2450.m3_DistanceResolution = Serial_RxPacket[22] + (Serial_RxPacket[23] << 8);

                RxState = 2;
                pRxPacketIndex = 0;
              }
            }
            break;

          case 2: // 检查包尾
            if (checkTailIndex == 0 && RxData == 0x55)
            {
              checkTailIndex = 1;
            }
            else if (checkTailIndex == 1 && RxData == 0xCC)
            {
              Serial_RxFlag = 1;
              RxState = 0;
              checkTailIndex = 0;
              d1 = calculateDistance(HiLink_2450.m1_xCoordinate, HiLink_2450.m1_yCoordinate);
              d2 = calculateDistance(HiLink_2450.m2_xCoordinate, HiLink_2450.m2_yCoordinate);
              d3 = calculateDistance(HiLink_2450.m3_xCoordinate, HiLink_2450.m3_yCoordinate);
              // 数据包完整接收，可以在这里处理数据
              // printTargetData();
            }
            else
            {
              RxState = 0;
              pRxHeadIndex = 0;
            }
            break;
          }
        }
        break;

      case UART_FIFO_OVF:
      case UART_BUFFER_FULL:
        uart_flush_input(UART_NUM);
        break;

      default:
        break;
      }
    }
  }

  free(dtmp);
  dtmp = NULL;
  vTaskDelete(NULL);
}

// 核心0任务：蓝牙数据接收
void Task_BluetoothReceive(void *pvParameters)
{
  while (1)
  {
    // for (int i = 0; i < 1; ++i)
    // {
    if (connected[0] && !clients[0]->isConnected())
    {
      Serial.printf("从设备 %d 断开，尝试重新连接...\n", 0 + 1);
      connected[0] = false;
      vTaskDelay(10 / portTICK_PERIOD_MS); // 1秒发送一次
      BLEAddress address(slaveMacs[0]);
      connectToSlave(0, address);
    }
    // }
    vTaskDelay(130 / portTICK_PERIOD_MS); // 1秒发送一次
  }
}
uint8_t txBuffer[sizeof(RadarPacket_t)]; // 发送缓冲区
// 核心0任务：蓝牙数据接收
int16_t prev_a = 0, prev_b = 0; // 存储上一次的值
int count3 = 0;
bool is_stable_for_3_cycles(int16_t current_a, int16_t current_b)
{
  // 连续相同的次数
  if (current_a == prev_a && current_b == prev_b)
  {
    count3++; // 值没变，计数器+1
    if (count3 >= 1)
    { // 已经连续 3 次相同（包括本次）
      count3 = 0;
      // prev_a = current_a;
      // prev_b = current_b;                       // 可选：重置计数器
      return true;
    }
  }
  else
  {
    count3 = 0;         // 值变化，重置计数器
    prev_a = current_a; // 更新存储的值
    prev_b = current_b;
  }

  return false;
}
int findMinIndex(uint16_t a, uint16_t b, uint16_t c)
{
  if (a <= b && a <= c)
  {
    return 1; // 第一个变量最小
  }
  else if (b <= a && b <= c)
  {
    return 2; // 第二个变量最小
  }
  else
  {
    return 3; // 第三个变量最小
  }
}

void Task_BluetoothReceive1(void *pvParameters)
{
  while (1)
  {
    Serial.print("deviceConnected =  ");
    Serial.println(deviceConnected);
    // Serial.println(connected[1] ? "已连接" : "未连接");
    if (deviceConnected)
    {
      int flag = findMinIndex(d1, d2, d3);
      if (flag == 1)
      {
        packet.x = HiLink_2450.m1_xCoordinate; // X坐标（毫米）
        packet.y = HiLink_2450.m1_yCoordinate; // Y坐标（毫米）
      }
      else if (flag == 2)
      {
        packet.x = HiLink_2450.m2_xCoordinate; // X坐标（毫米）
        packet.y = HiLink_2450.m2_yCoordinate; // Y坐标（毫米）
      }
      else if (flag == 3)
      {
        packet.x = HiLink_2450.m3_xCoordinate; // X坐标（毫米）
        packet.y = HiLink_2450.m3_yCoordinate; // Y坐标（毫米）
      }
      // packet.packet_id++;
      // packet.x = HiLink_2450.m1_xCoordinate; // X坐标（毫米）
      // packet.y = HiLink_2450.m1_yCoordinate; // Y坐标（毫米）
      packet.x1 = x2;
      packet.y1 = y2;
      if (is_stable_for_3_cycles(packet.x, packet.y))
      {
        packet.x = 0;
        packet.y = -32767;
      }
      if(door_flag == 1){
        packet.id = 1;
        door_flag = 0;
      }
      Serial.printf("id=%d\n", packet.id);
      memcpy(txBuffer, &packet, sizeof(packet));
      pCharacteristic1->setValue(txBuffer, sizeof(txBuffer));
      pCharacteristic1->notify();
      packet.id = 0;
      Serial.printf("x1=%.3f, y1=%.3f,x2=%.3f,y2=%.3f\n", packet.x1 / 1000.0, packet.y1 / 1000.0, packet.x / 1000.0, packet.y / 1000.0);
    }
    vTaskDelay(260 / portTICK_PERIOD_MS); // 1秒发送一次
  }
}

void Task_BluetoothReceive2(void *pvParameters)
{
  while (1)
  {
    uint8_t uid[7]; // 存储读取的 UID
    uint8_t uidLength;

    // 检测是否有卡片靠近
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength))
    {
      Serial.print("读取到卡 UID: ");
      for (uint8_t i = 0; i < uidLength; i++)
      {
        Serial.print(uid[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // 检查是否为授权卡
      if (checkAuthorized(uid, uidLength))
      {
        Serial.println("授权成功，门已解锁。");
        door_flag = 1;
      }
      else
      {
        Serial.println("未授权的卡片！");
      }  
    }
      vTaskDelay(500 / portTICK_PERIOD_MS); // 1秒发送一次
  }
}
  void setup()
  {
    Serial.begin(115200);
    // 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = 256000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    // 安装UART驱动
    uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart1_queue, 0);
    uart_param_config(UART_NUM, &uart_config);

    // 设置UART引脚（根据实际连接修改）
    uart_set_pin(UART_NUM, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    Serial.println("主设备启动");
    BLEDevice::init("ESP32_MASTER");

    Serial.print("从设备 MAC 地址: ");
    Serial.println(BLEDevice::getAddress().toString().c_str());

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID1);
    pCharacteristic1 = pService->createCharacteristic(CHARACTERISTIC_UUID1, BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic1->addDescriptor(new BLE2902());
    pService->start();
    pServer->getAdvertising()->start();

    for (int i = 0; i < 1; ++i)
    {
      BLEAddress address(slaveMacs[i]);
      connectToSlave(i, address);
      // Serial.print("error: ");
      delay(100); // 稍作延迟，避免并发连接冲突
    }

    // 初始化 I2C 接口
    Wire.begin(); // 默认使用 SDA=21, SCL=22（ESP32 的默认 I2C 引脚）
    // 初始化 PN532
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata)
    {
      Serial.println("未检测到 PN532 模块，请检查接线！");
      while (1)
        ; // 停止执行
    }
    Serial.print("PN532 固件版本: 0x");
    Serial.println(versiondata, HEX);

    // 配置 PN532 以读取 Mifare 卡
    nfc.SAMConfig();
    Serial.println("NFC 门禁系统启动...");
    xTaskCreatePinnedToCore(
        Task_BluetoothReceive, // 任务函数
        "BluetoothReceive",    // 任务名称
        4096,                  // 堆栈大小
        NULL,                  // 参数
        1,                     // 优先级
        NULL,                  // 任务句柄
        0                      // 运行在核心0
    );
    xTaskCreatePinnedToCore(
        Task_BluetoothReceive1, // 任务函数
        "BluetoothReceive1",    // 任务名称
        4096,                   // 堆栈大小
        NULL,                   // 参数
        3,                      // 优先级
        NULL,                   // 任务句柄
        1                       // 运行在核心0
    );
    xTaskCreatePinnedToCore(
        Task_BluetoothReceive2, // 任务函数
        "BluetoothReceive2",    // 任务名称
        4096,                   // 堆栈大小
        NULL,                   // 参数
        3,                      // 优先级
        NULL,                   // 任务句柄
        1                       // 运行在核心0
    );
    xTaskCreatePinnedToCore(uart1_task, "uart_task", 4096, NULL, 12, NULL, 1); // Core 1
  }

  void loop()
  {
    vTaskDelete(NULL); // 删除主循环任务
  }
