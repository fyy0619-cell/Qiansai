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
// 数据包结构（使用数组缓冲区）
#pragma pack(push, 1) // 1字节对齐
typedef struct
{
  uint8_t header[2]; // 包头：0xAA, 0xFF
  uint8_t id;
  uint16_t packet_id; // 包序号
  int16_t x;          // X坐标
  int16_t y;          // Y坐标
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
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
  int16_t x1;
  int16_t y1;
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
// 通用的服务和特征 UUID
#define SERVICE_UUID "a8e4c3b2-5d7e-4f8a-9b0c-3d1e2f4a5b6c"
#define CHARACTERISTIC_UUID "d9c8b7a6-5e4f-3d2c-1b0a-9c8d7e6f5a4b"
#define SERVICE_UUID1 "5d9f680d-aa9a-43d7-9d88-34049e7663e1"
#define CHARACTERISTIC_UUID1 "bcf0e6da-9d88-4497-bad3-2fc93acb2f71"
// #define SERVICE_UUID1 "a8e4c3b2-5d7e-4f8a-9b0c-3d1e2f4a5b6c"
// #define CHARACTERISTIC_UUID1 "d9c8b7a6-5e4f-3d2c-1b0a-9c8d7e6f5a4b"
// 从设备 MAC 地址（你可以替换成第二个从机的实际地址）
const char *slaveMacs[] = {
    "6c:c8:40:56:19:22", // 设备 1
  //  "68:25:dd:f0:59:32"  // "78:42:1c:6a:fc:7e"  // 从设备 2（示例地址，替换成真实 MAC）
};

BLEServer *pServer = nullptr;
BLECharacteristic *pCharacteristic1 = nullptr;
bool deviceConnected = false;

BLEClient *clients[1] = {nullptr};
BLERemoteCharacteristic *remoteChars[1] = {nullptr};
bool connected[1] = {false};
int16_t a1,b1,a2,b2;
uint8_t id;
void notifyCallback(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{

  if (length == sizeof(rxBuffer))
  {
    id = pData[2];
    a2 = (pData[6] << 8) | pData[5]; // 大端序转小端序
    b2 = (pData[8] << 8) | pData[7];
    a1 = (pData[10] << 8) | pData[9]; // 大端序转小端序
    b1 = (pData[12] << 8) | pData[11];
    // Serial.printf("X1=%.2fm, Y1=%.2fm,X2=%.2fm, Y2=%.2fm ,X3=%.2fm, Y3=%.2fm\n",
    //               a1 / 1000.0,
    //               b1 / 1000.0,
    //               a2 / 1000.0,
    //               b2 / 1000.0,
    //               HiLink_2450.m1_xCoordinate/ 1000.0,
    //               HiLink_2450.m1_yCoordinate/ 1000.0
    //             );
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
    vTaskDelay(140 / portTICK_PERIOD_MS); // 1秒发送一次
  }
}
uint8_t txBuffer[sizeof(RadarPacket_t)]; // 发送缓冲区
// 核心0任务：蓝牙数据接收
int16_t prev_a = 0, prev_b = 0;  // 存储上一次的值
int count3 = 0;  
bool is_stable_for_3_cycles(int16_t current_a, int16_t current_b) {
                     // 连续相同的次数
    if (current_a == prev_a && current_b == prev_b) {
        count3++;                            // 值没变，计数器+1
        if (count3 >= 1) {                   // 已经连续 3 次相同（包括本次）
            count3 = 0;  
            // prev_a = current_a;   
            // prev_b = current_b;                       // 可选：重置计数器
            return true;
        }
    } else {
        count3 = 0;                          // 值变化，重置计数器
        prev_a = current_a;                 // 更新存储的值
        prev_b = current_b;
    }

    return false;
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
            packet.id = id;
            packet.x = a1; // X坐标（毫米）
            packet.y = b1; // Y坐标（毫米）
            packet.x1 = a2;
            packet.y1 = b2;
            packet.x2 = HiLink_2450.m1_xCoordinate;
            packet.y2 = HiLink_2450.m1_yCoordinate;
            if(is_stable_for_3_cycles(packet.x2,packet.y2)){
              packet.x2 = 0;
              packet.y2 = -32767;
            }
            memcpy(txBuffer, &packet, sizeof(packet));
            pCharacteristic1->setValue(txBuffer, sizeof(txBuffer));
            pCharacteristic1->notify();
            Serial.printf("x1=%.3f, y1=%.3f,x2=%.3f,y2=%.3f,x3=%.3f, y3=%.3f\n", 
              packet.x/1000.0,  packet.y/1000.0,   packet.x1/1000.0,  packet.y1/1000.0,  packet.x2/1000.0,  packet.y2/1000.0);
        }
        vTaskDelay(220 / portTICK_PERIOD_MS); // 1秒发送一次
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
      4096,                  // 堆栈大小
      NULL,                  // 参数
      3,                     // 优先级
      NULL,                  // 任务句柄
      1                      // 运行在核心0
  );
  xTaskCreatePinnedToCore(uart1_task, "uart_task", 4096, NULL, 12, NULL, 1); // Core 1
}

void loop()
{
  vTaskDelete(NULL); // 删除主循环任务
}
