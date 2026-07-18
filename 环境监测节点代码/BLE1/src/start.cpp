#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>
#include <math.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


#define SERVICE_UUID "5d9f680d-aa9a-43d7-9d88-34049e7663e0"
#define CHARACTERISTIC_UUID "bcf0e6da-9d88-4497-bad3-2fc93acb2f78"

BLEServer *pServer = nullptr;
BLECharacteristic *pCharacteristic = nullptr;
bool deviceConnected = false;

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
                            d1 = calculateDistance(HiLink_2450.m1_xCoordinate, HiLink_2450.m1_yCoordinate);
                            d2 = calculateDistance(HiLink_2450.m2_xCoordinate, HiLink_2450.m2_yCoordinate);
                            d3 = calculateDistance(HiLink_2450.m3_xCoordinate, HiLink_2450.m3_yCoordinate);
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
const uint8_t setDelayCommand[] = {
    0xFD, 0xFC, 0xFB, 0xFA, // 帧头
    0x04, 0x00,             // 数据长度(8字节)
    0xFF, 0x00,             // 命令字(0x0007)
    0x01, 0x00,             // 参数ID(0x0004)
    0x04, 0x03, 0x02, 0x01  // 帧尾
};

const uint8_t setDelayCommand1[] = {
    0xFD, 0xFC, 0xFB, 0xFA, // 帧头
    0x02, 0x00,             // 数据长度(8字节)
    0x90, 0x00,             // 命令字(0x0007)
    0x04, 0x03, 0x02, 0x01  // 帧尾
};

const uint8_t setDelayCommand2[] = {
    0xFD, 0xFC, 0xFB, 0xFA, // 帧头
    0x04, 0x00,             // 数据长度(8字节)
    0x00, 0x00,             // 命令字(0x0007)
    0x04, 0x03, 0x02, 0x01  // 帧尾
};

const uint8_t setDelayCommand3[] = {
    0xFD, 0xFC, 0xFB, 0xFA, // 帧头
    0x02, 0x00,             // 数据长度(8字节)
    0xFE, 0x00,             // 命令字(0x0007)
    0x04, 0x03, 0x02, 0x01  // 帧尾
};
// 数据包结构（使用数组缓冲区）
#pragma pack(push, 1) // 1字节对齐
typedef struct
{
    uint8_t header[2]; // 包头：0xAA, 0xFF
    uint8_t id;
    uint16_t packet_id; // 包序号
    int16_t x;          // X坐标
    int16_t y;          // Y坐标
} RadarPacket_t;
#pragma pack(pop)
// 蓝牙数据发送任务
// 全局缓冲区
uint8_t txBuffer[sizeof(RadarPacket_t)]; // 发送缓冲区
int16_t prev_a = 0, prev_b = 0;          // 存储上一次的值
int count3 = 0;
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
bool is_stable_for_3_cycles(int16_t current_a, int16_t current_b)
{
    // 连续相同的次数
    if (current_a == prev_a && current_b == prev_b)
    {
        count3++; // 值没变，计数器+1
        if (count3 >= 1)
        {               // 已经连续 3 次相同（包括本次）
            count3 = 0; // 可选：重置计数器
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
void bt_task(void *pvParameters)
{
    RadarPacket_t packet = {
        .header = {0xAA, 0xFF},
        .id = 1,
        .packet_id = 0,
        .x = 0,
        .y = 0,
        // .checksum = 0
    };
    while (1)
    {
        if (deviceConnected)
        {
            int flag = findMinIndex(d1, d2, d3);
            if (flag == 1)
            {
                packet.x = HiLink_2450.m1_xCoordinate; // X坐标（毫米）
                packet.y = HiLink_2450.m1_yCoordinate; // Y坐标（毫米）
            }else if(flag == 2){
                packet.x = HiLink_2450.m2_xCoordinate; // X坐标（毫米）
                packet.y = HiLink_2450.m2_yCoordinate; // Y坐标（毫米）
            }else if(flag ==3){
                packet.x = HiLink_2450.m3_xCoordinate; // X坐标（毫米）
                packet.y = HiLink_2450.m3_yCoordinate; // Y坐标（毫米）
            }
            if (is_stable_for_3_cycles(packet.x, packet.y))
            {
                packet.x = 0;
                packet.y = -32767;
            }
            memcpy(txBuffer, &packet, sizeof(packet));
            pCharacteristic->setValue(txBuffer, sizeof(txBuffer));
            pCharacteristic->notify();

        }
        vTaskDelay(300 / portTICK_PERIOD_MS); // 10Hz发送频率  200ms周期
    }
}

void setup()
{
    Serial.begin(115200);
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

    BLEDevice::init("ESP32_SLAVE_1");

    Serial.print("从设备 MAC 地址: ");
    Serial.println(BLEDevice::getAddress().toString().c_str());

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    pServer->getAdvertising()->start();

    // 创建UART任务
    // xTaskCreate(uart1_task, "uart1_task", 4096, NULL, 12, &uart1_task_handle);
    xTaskCreatePinnedToCore(uart1_task, "uart_task", 4096, NULL, 12, NULL, 1); // Core 1
    xTaskCreatePinnedToCore(bt_task, "bt_task", 4096, NULL, 10, NULL, 0);      // Core 0
    Serial.println("ESP32 LD2450雷达数据接收已启动");
    // 初始化经典蓝牙
    Serial.println("经典蓝牙已启动，名称: ESP32-LD2450");

}

void loop()
{
    // 主循环可以执行其他任务
    vTaskDelete(NULL); // 删除主循环任务
}