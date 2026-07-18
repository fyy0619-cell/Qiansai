#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include "uwb.h"
#include <ws2812b.h>
#include <mpu6050_v1.h>
#include <stdint.h>
#include <stdbool.h>
// #include <HardwareSerial.h>
// 数据包结构（使用数组缓冲区）
// 使用 UART2（TX=16, RX=17）

HardwareSerial SerialPort(2);
// 创建一个 NeoPixel 对象
// 定义引脚和LED数量

#define g 9.80665     // 只控制一个LED灯
#pragma pack(push, 1) // 1字节对齐
typedef struct
{
  uint8_t header[2]; // 包头：0xAA, 0xFF
  uint8_t id = 0;
  uint16_t packet_id; // 包序号
  int16_t x;          // X坐标
  int16_t y;          // Y坐标
  int16_t x1;         // X坐标
  int16_t y1;         // Y坐标
  int16_t x2;         // X坐标
  int16_t y2;         // Y坐标
} RadarPacket_t;
#pragma pack(pop)

uint8_t rxBuffer[sizeof(RadarPacket_t)]; // 接收缓冲区
RadarPacket_t packet;
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

// 通用的服务和特征 UUID
// #define SERVICE_UUID "a8e4c3b2-5d7e-4f8a-9b0c-3d1e2f4a5b6c"
// #define CHARACTERISTIC_UUID "d9c8b7a6-5e4f-3d2c-1b0a-9c8d7e6f5a4b"

#define SERVICE_UUID "5d9f680d-aa9a-43d7-9d88-34049e7663e1"
#define CHARACTERISTIC_UUID "bcf0e6da-9d88-4497-bad3-2fc93acb2f71"
// 从设备 MAC 地址（你可以替换成第二个从机的实际地址）
const char *slaveMacs[] = {
    "68:25:dd:f0:59:32", // 设备 2
                         // "78:42:1c:6a:fc:7e"  // 从设备 2（示例地址，替换成真实 MAC）
};

BLEClient *clients[1] = {nullptr};
BLERemoteCharacteristic *remoteChars[1] = {nullptr};
bool connected[1] = {false};
double distance1, distance2, distance3 = 0;
int flag = 0;
int count1, count2 = 0;
int id1 = 2;
uint8_t id;
double a1, b1, a2, b2, a3, b3;
int flag_convert = 0;
void notifyCallback(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  if (length == sizeof(rxBuffer))
  {
    id = pData[2];
    int16_t x1 = (pData[6] << 8) | pData[5]; // 大端序转小端序
    int16_t y1 = (pData[8] << 8) | pData[7];
    int16_t x2 = (pData[10] << 8) | pData[9]; // 大端序转小端序
    int16_t y2 = (pData[12] << 8) | pData[11];
    int16_t x3 = (pData[14] << 8) | pData[13]; // 大端序转小端序
    int16_t y3 = (pData[16] << 8) | pData[15];
    flag_convert = 1;
    a1 = x1 / 1000.0;
    b1 = y1 / 1000.0;
    a2 = x2 / 1000.0;
    b2 = y2 / 1000.0;
    a3 = x3 / 1000.0;
    b3 = y3 / 1000.0;
    distance1 = calculateDistance(a1, b1);
    distance2 = calculateDistance(a2, b2);
    distance3 = calculateDistance(a3, b3);
    Serial.printf("X1=%.2fm,Y1=%.2fm,Mwave1=%.2fm,   X2=%.2fm,Y2=%.2fm,Mwave2=%.2fm,   X3=%.2fm,Y3=%.2f,Mwave3=%.2fm,  uwb=%.2fm, isuwb=%d, 差1=%.2fm, 差2=%.2fm, 差3=%.2fm\n",
                  a1, b1, distance1,
                  a2, b2, distance2,
                  a3, b3, distance3,
                  distance0, is_uwb,
                  abs(distance1 - distance0),
                  abs(distance2 - distance0),
                  abs(distance3 - distance0));
    Serial.println();
  }

  else
  {
    Serial.println("收到的数据长度不正确");
  }
}
// // 定义PI常量
// #ifndef M_PI
// #define M_PI 3.14159265358979323846
// #endif

// 结构体用于存储笛卡尔坐标
struct Cartesian
{
  double x;
  double y;
};
Cartesian rotateDirect(double x, double y, double degrees)
{
  double radians = -degrees * M_PI / 180.0;
  double c = cos(radians);
  double s = sin(radians);
  return {x * c - y * s, x * s + y * c};
}
struct uart
{
  double x1;
  double y1;
  double x2;
  double y2;
  double x3;
  double y3;
  int flag1 = 0;
  int flag2 = 0;
  int flag3 = 0;
  int flag4 = 0;
  double x;
  double y;
};
uint8_t c;
void Task_BluetoothReceive2(void *pvParameters)
{
  while (1)
  {
    // if (flag_convert == 1)
    // {
    if (SerialPort.available() > 0)
    {
      // 方式1：读取单个字符
      c = SerialPort.read();

      if (c == 1)
      {
        Serial.print("Received: ");
        Serial.println(c);
        door1 = 1;
        strip.setPixelColor(0, strip.Color(0, 250, 0)); // 绿色 RGB(0,50,0)
        strip.show();
      }
    }
    uart data;
    Cartesian rotated1 = rotateDirect(a1, b1, 120.0);
    Cartesian rotated2 = rotateDirect(a2, b2, -120.0);
    data.x1 = rotated1.x + 0.6;
    data.y1 = rotated1.y;
    data.x2 = rotated2.x - 0.6;
    data.y2 = rotated2.y;
    data.x3 = a3;
    data.y3 = b3 + 0.5;
    if (is_uwb == 1)
    {
      if (abs(distance1 - dis0) <= 0.8)
      {
        data.x = data.x1;
        data.y = data.y1;
        data.flag1 = 1;
      }
      else if (abs(distance2 - dis0) <= 0.8)
      {
        data.x = data.x2;
        data.y = data.y2;
        data.flag2 = 1;
      }
      else if (abs(distance3 - dis0) <= 0.8)
      {
        data.x = data.x3;
        data.y = data.y3;
        data.flag3 = 1;
      }
      // else
      // {
      //   data.flag4 = 1;
      // }
    }
  if (is_uwb == 1 && data.flag1 == 1)
  {
    SerialPort.print(door1);
    SerialPort.print(",");
    SerialPort.print(is_uwb);
    SerialPort.print(",");
    SerialPort.print(data.x, 2);
    SerialPort.print(",");
    SerialPort.print(data.y, 2);
    SerialPort.print(",");
    SerialPort.print(data.x2, 2);
    SerialPort.print(",");
    SerialPort.print(data.y2, 2);
    SerialPort.print(",");
    SerialPort.print(data.x3, 2);
    SerialPort.print(",");
    SerialPort.print(data.y3, 2);
    SerialPort.println(";");
  }
  else if (is_uwb == 1 && data.flag2 == 1)
  {
    SerialPort.print(door1);
    SerialPort.print(",");
    SerialPort.print(is_uwb);
    SerialPort.print(",");
    SerialPort.print(data.x, 2);
    SerialPort.print(",");
    SerialPort.print(data.y, 2);
    SerialPort.print(",");
    SerialPort.print(data.x1, 2);
    SerialPort.print(",");
    SerialPort.print(data.y1, 2);
    SerialPort.print(",");
    SerialPort.print(data.x3, 2);
    SerialPort.print(",");
    SerialPort.print(data.y3, 2);
    SerialPort.println(";");
  }
  else if (is_uwb == 1 && data.flag3 == 1)
  {
    SerialPort.print(door1);
    SerialPort.print(",");
    SerialPort.print(is_uwb);
    SerialPort.print(",");
    SerialPort.print(data.x, 2);
    SerialPort.print(",");
    SerialPort.print(data.y, 2);
    SerialPort.print(",");
    SerialPort.print(data.x1, 2);
    SerialPort.print(",");
    SerialPort.print(data.y1, 2);
    SerialPort.print(",");
    SerialPort.print(data.x2, 2);
    SerialPort.print(",");
    SerialPort.print(data.y2, 2);
    SerialPort.println(";");
  }

  else if (is_uwb == 0)
  {
    SerialPort.print(door1);
    SerialPort.print(",");
    SerialPort.print(is_uwb);
    SerialPort.print(",");
    SerialPort.print(data.x1, 2);
    SerialPort.print(",");
    SerialPort.print(data.y1, 2);
    SerialPort.print(",");
    SerialPort.print(data.x2, 2);
    SerialPort.print(",");
    SerialPort.print(data.y2, 2);
    SerialPort.print(",");
    SerialPort.print(data.x3, 2);
    SerialPort.print(",");
    SerialPort.print(data.y3, 2);
    SerialPort.println(";");
  }
  // }
  // flag_convert = 0;
  vTaskDelay(220 / portTICK_PERIOD_MS); // 1秒发送一次
}
}
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
    Serial.printf("44444\n");
    return false;
  }

  remoteChars[index] = service->getCharacteristic(CHARACTERISTIC_UUID);
  if (!remoteChars[index])
  {
    Serial.println("未找到特征值");
    clients[index]->disconnect();
    Serial.printf("33333\n");
    return false;
  }

  if (remoteChars[index]->canNotify())
  {
    remoteChars[index]->registerForNotify(notifyCallback);
    connected[index] = true;
    Serial.printf("11111\n");
    return true;
  }
  Serial.printf("22222\n");
  return false;
}
// 核心0任务：蓝牙数据接收
void Task_BluetoothReceive(void *pvParameters)
{
  while (1)
  {
    for (int i = 0; i < 1; ++i)
    {
      if (connected[i] && !clients[i]->isConnected())
      {
        Serial.printf("从设备 %d 断开，尝试重新连接...\n", i + 1);
        connected[i] = false;
        vTaskDelay(10 / portTICK_PERIOD_MS); //
        BLEAddress address(slaveMacs[i]);
        connectToSlave(i, address);
      }
    }
    vTaskDelay(150 / portTICK_PERIOD_MS); // 1秒发送一次
  }
}

// 核心1任务：数据处理和打印
void Task_DataProcess(void *pvParameters)
{
  // #ifdef INITIATOR
  while (1)
  {
    initiator();
  }
}
void Task_MPU6050_data(void *pvParameters)
{
  while (1)
  {
    if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
    {
      // 获取线性加速度
      mpu.dmpGetQuaternion(&q, fifoBuffer);
      mpu.dmpGetGravity(&gravity, &q);
      mpu.dmpGetAccel(&aa, fifoBuffer);
      mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
      mpu.dmpGetYawPitchRoll(pry, &q, &gravity); // 姿态
    }
    // show_pry();
    arealx = float(aaReal.x);
    arealy = float(aaReal.y);
    arealz = float(aaReal.z);
    double kalAx = kalmanX.update(arealx / 16384);
    double kalAy = kalmanX.update(arealy / 16384);
    double kalAz = kalmanX.update(arealz / 16384);
    // x+=kalAx*g;
    // y+=kalAy*g;
    // z+=kalAz*g;
    // 使用卡尔曼滤波对加速度数据进行滤波
    kalAx = kalAx * g;
    kalAy = kalAy * g;
    kalAz = kalAz * g;
    aax = (int)(kalAx * 100) / 100.0; // 截取两位小数
    aay = (int)(kalAy * 100) / 100.0; // 截取两位小数
    aaz = (int)(kalAz * 100) / 100.0; // 截取两位小数
    // Serial.printf("ax: %.3f", aax);
    // Serial.print("m/s2   ");
    // Serial.printf("ay: %.3f", aay);
    // Serial.print("m/s2   ");
    // Serial.printf("az: %.3f", aaz);
    // Serial.println("m/s2   ");
    if (abs(aax) > 0.1)
    {
      digitalWrite(4, HIGH);
      vTaskDelay(5 / portTICK_PERIOD_MS); // 10ms间隔
      Serial.println("车辆擦碰!!!");
    }
    else
    {
      digitalWrite(4, LOW);
    }
    vTaskDelay(180 / portTICK_PERIOD_MS); // 10ms间隔
  }
}
void Task_BluetoothReceive1(void *pvParameters)
{
  while (1)
  {
    // if ((abs(distance1 - distance0) <= 0.7 || abs(distance2 - distance0) <= 0.7 || abs(distance3 - distance0) <= 0.7) && is_uwb == 1)
    // {
    //   id1 = 1;
    // }
    // else if ((abs(distance1 - distance0) > 1.0 || abs(distance2 - distance0) > 1.0 || abs(distance3 - distance0) > 1.0) && is_uwb == 0)
    // {
    //   id1 = 0;
    // }
    if (id == 1)
    {
      door1 = 1;
      strip.setPixelColor(0, strip.Color(0, 250, 0)); // 绿色 RGB(0,50,0)
      strip.show();
    }
    if (flag == 0 && distance0 >= 0 && distance0 <= 1.85 && is_uwb == 1 && (abs(distance1 - dis0) <= 0.8 || abs(distance2 - dis0) <= 0.8 || abs(distance3 - dis0) <= 0.8))
    {
      strip.setPixelColor(0, strip.Color(0, 250, 0)); // 绿色 RGB(0,50,0)
      strip.show();
      door1 = 1;
      flag = 1;
    }
    else if (flag == 1 && distance0 >= 2.65 && is_uwb == 1 && (abs(distance1 - dis0) <= 0.8 || abs(distance2 - dis0) <= 0.8 || abs(distance3 - dis0) <= 0.8))
    {
      strip.setPixelColor(0, strip.Color(250, 0, 0)); // 红色 RGB(50,0,0)
      strip.show();
      door1 = 0; // 发送数据到 LED
      flag = 0;
    }
    vTaskDelay(90 / portTICK_PERIOD_MS); // 10ms间隔
    // Serial.print("distance0= ");
    // Serial.println(distance0);
    
    // Serial.print("dis0= ");
    // Serial.println(dis0);
  }
}
void setup()
{
  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH);
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  Serial.begin(115200);
  SerialPort.begin(115200, SERIAL_8N1, 16, 17); // 初始化串口
  spiBegin(UWB_IRQ, UWB_RST);
  spiSelect(UWB_SS);
  delay(2);
  start_uwb();
  delay(2);
  mpu_init(); // 初始化陀螺仪
  delay(2);
  strip.begin();
  strip.setPixelColor(0, strip.Color(250, 0, 0)); // 绿色 RGB(0,50,0)
  strip.show();                                  // 初始化所有像素为关灯状态
  Serial.println("主设备启动");
  BLEDevice::init("ESP32_MASTER1");
  Serial.print("从设备 MAC 地址: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());
  for (int i = 0; i < 1; ++i)
  {
    BLEAddress address(slaveMacs[i]);
    connectToSlave(i, address);
    Serial.print("error: ");
    delay(50); // 稍作延迟，避免并发连接冲突
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
      Task_DataProcess, // 任务函数
      "DataProcess",    // 任务名称
      16384,            // 堆栈大小
      NULL,             // 参数
      1,                // 优先级
      NULL,             // 任务句柄
      1                 // 运行在核心1
  );

  xTaskCreatePinnedToCore(
      Task_MPU6050_data, // 任务函数
      "MPU6050_data",    // 任务名称
      4096,              // 堆栈大小
      NULL,              // 参数
      1,                 // 优先级
      NULL,              // 任务句柄
      0                  // 运行在核心0
  );
  xTaskCreatePinnedToCore(
      Task_BluetoothReceive1, // 任务函数
      "BluetoothReceive1",    // 任务名称
      4096,                   // 堆栈大小
      NULL,                   // 参数
      1,                      // 优先级
      NULL,                   // 任务句柄
      0                       // 运行在核心0
  );
  xTaskCreatePinnedToCore(
      Task_BluetoothReceive2, // 任务函数
      "BluetoothReceive2",    // 任务名称
      4096,                   // 堆栈大小
      NULL,                   // 参数
      1,                      // 优先级
      NULL,                   // 任务句柄
      0                       // 运行在核心0
  );
}

void loop()
{
  vTaskDelete(NULL); // 删除主循环任务
}
