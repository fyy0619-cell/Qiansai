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
// 使用 UART2（RX=16, TX=17）

HardwareSerial SerialPort(2);
// 创建一个 NeoPixel 对象
// 定义引脚和LED数量

#define g 9.80665     // 只控制一个LED灯
#pragma pack(push, 1) // 1字节对齐
typedef struct
{
  uint8_t id = 0;
  int16_t x;  // X坐标
  int16_t y;  // Y坐标
  int16_t x1; // X坐标
  int16_t y1; // Y坐标
  int16_t x2; // X坐标
  int16_t y2; // Y坐标
  uint8_t dis;
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
double distance1, distance2, distance3, distance4 = 0;
int flag = 0;
int count1, count2 = 0;
int id1 = 2;
uint8_t id;
volatile double a1, b1, a2, b2, a3, b3, a4, b4, dis2;
int flag_convert = 0;
volatile double uwb_dis = 0;
volatile double di0, distan0 = 0;
volatile uint8_t isuwb, isuwb1 = 0;
void notifyCallback(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  if (length == sizeof(rxBuffer))
  {
    id = pData[0];
    int16_t x1 = (pData[2] << 8) | pData[1]; // 大端序转小端序
    int16_t y1 = (pData[4] << 8) | pData[3];
    int16_t x2 = (pData[6] << 8) | pData[5]; // 大端序转小端序
    int16_t y2 = (pData[8] << 8) | pData[7];
    int16_t x3 = (pData[10] << 8) | pData[9]; // 大端序转小端序
    int16_t y3 = (pData[12] << 8) | pData[11];
    // int16_t x4 = (pData[14] << 8) | pData[13]; // 大端序转小端序
    // int16_t y4 = (pData[16] << 8) | pData[15];
    dis2 = pData[13];
    flag_convert = 1;
    a1 = x1 / 1000.0;
    b1 = y1 / 1000.0;
    a2 = x2 / 1000.0;
    b2 = y2 / 1000.0;
    a3 = x3 / 1000.0;
    b3 = y3 / 1000.0;
    // b4 = x4 / 1000.0;
    // b4 = y4 / 1000.0;
    distance1 = calculateDistance(a1, b1);
    distance2 = calculateDistance(a2, b2);
    distance3 = calculateDistance(a3, b3);
    // distance4 = calculateDistance(a4, b4);
    // if(uwb_dis )
    // Serial.printf("X1=%.2fm,Y1=%.2fm,Mwave1=%.2fm,   X2=%.2fm,Y2=%.2fm,Mwave2=%.2fm,   X3=%.2fm,Y3=%.2f,Mwave3=%.2fm,  uwb=%.2fm, isuwb=%d, 差1=%.2fm, 差2=%.2fm, 差3=%.2fm\n",
    //               a1, b1, distance1,
    //               a2, b2, distance2,
    //               a3, b3, distance3,
    //               distance0, is_uwb,
    //               abs(distance1 - distance0),
    //               abs(distance2 - distance0),
    //               abs(distance3 - distance0));
    // Serial.println();
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
// 返回最大值的序号
int findMaxIndex(int a, int b, int c)
{
  int max = a;   // 先假设a是最大的
  int index = 1; // 默认a的序号是1
  if (b > max)
  { // 比较b和当前最大值
    max = b;
    index = 2; // 更新为b的序号
  }
  if (c > max)
  { // 比较c和当前最大值
    max = c;
    index = 3; // 更新为c的序号
  }
  return index;
}
// 返回最小值的序号
int findMinIndex(int a, int b, int c)
{
  int min = a;   // 先假设a是最小的
  int index = 1; // 默认a的序号是1

  if (b < min)
  { // 比较b和当前最小值
    min = b;
    index = 2; // 更新为b的序号
  }
  if (c < min)
  { // 比较c和当前最小值
    min = c;
    index = 3; // 更新为c的序号
  }

  return index;
}
struct uart
{
  volatile double x1;
  volatile double y1;
  volatile double x2;
  volatile double y2;
  volatile double x3;
  volatile double y3;
  volatile double v1;
  volatile double v2;
  volatile double v3;
  volatile int flag1 = 0;
  volatile int flag2 = 0;
  volatile int flag3 = 0;
  volatile int flag4 = 0;
  volatile int lastflag = 0;
  volatile double x;
  volatile double y;
};
uint8_t c;
double is_flag = 0;
int alert_flag = 0;

// 检测距离变化的函数
int detectDistanceChange(double currentDistance, double previousDistance)
{
  // 定义小幅度变化的阈值
  const double SMALL_CHANGE_THRESHOLD = 0.07;
  // 定义大幅度变化的阈值
  const double LARGE_CHANGE_THRESHOLD = 0.07;

  // 计算距离变化量
  double change = abs(currentDistance - previousDistance);

  if (change < SMALL_CHANGE_THRESHOLD)
  {
    // 小幅度变化或无变化
    return 0;
  }
  else if (change >= LARGE_CHANGE_THRESHOLD)
  {
    // 大幅度变化
    return 1;
  }
  else
  {
    // 中等幅度变化（根据需求可以返回0或1）
    return 0;
  }
}
volatile int car_change = 0;
int progress = 0;

typedef struct
{
  volatile double x, y;           // 当前坐标
  volatile double prev_x, prev_y; // 上一帧坐标
  volatile double prev_distance;  // 上一帧距离中心点的距离
  volatile int confidence;        // 当前置信度
  volatile double v;
} Pedestrian;

// 计算点到(0,0)的欧几里得距离
double calc_distance(double x, double y)
{
  return sqrt(x * x + y * y);
}

// 距离到最大置信度映射（非常细化）
int distance_to_confidence(double d)
{
  if (d >= 3.0)
    return 0;
  if (d <= 0.8)
    return 99;
  return (int)(((3.0 - d) / (3.0 - 0.8)) * 99);
}

void update_speed(Pedestrian *p, double delta_time)
{
  // 计算当前帧与中心点(0,0)的距离
  double distance_to_center = sqrt(p->x * p->x + p->y * p->y);

  // 仅当当前坐标在距离中心点3米以内时，才更新速度
  if (distance_to_center <= 3.0)
  {
    double dx = p->x - p->prev_x;
    double dy = p->y - p->prev_y;
    double distance = sqrt(dx * dx + dy * dy);

    p->v = distance / delta_time;

    // 更新上一帧坐标
    p->prev_x = p->x;
    p->prev_y = p->y;

    // 更新上一帧与中心点的距离
    p->prev_distance = distance_to_center;
  }
  else
  {
    // 如果超出范围，可以选择设置速度为 0，或保持原值
    p->v = 0.0;
  }
}

// 更新单个行人的置信度
int update_confidence(Pedestrian *p)
{
  double d = calc_distance(p->x, p->y);
  double delta_d = fabs(d - p->prev_distance);
  p->prev_distance = d;

  // 过远直接为0
  if (d > 3.0)
  {
    p->confidence = 0;
    return p->confidence;
  }

  // 小于碰撞阈值，设为100
  if (d < 0.75)
  {
    p->confidence = 100;
    return p->confidence;
  }

  int target_conf = distance_to_confidence(d);

  if (delta_d < 0.02)
  {
    // 静止缓慢降低置信度（不低于映射值）
    if (p->confidence > target_conf)
      p->confidence -= 2;
    if (p->confidence < target_conf)
      p->confidence = target_conf;
  }
  else
  {
    // 移动则逐步恢复置信度（不高于映射值）
    if (p->confidence < target_conf)
      p->confidence += 3;
    if (p->confidence > target_conf)
      p->confidence = target_conf;
  }

  return p->confidence;
}

// 找到离中心最近的行人索引
int find_nearest_index(Pedestrian *peds, int count)
{
  int nearest = 0;
  double min_d = calc_distance(peds[0].x, peds[0].y);
  for (int i = 1; i < count; ++i)
  {
    double d = calc_distance(peds[i].x, peds[i].y);
    if (d < min_d)
    {
      min_d = d;
      nearest = i;
    }
  }
  return nearest;
}

uart data;
volatile double lastx1, lasty1, lastx2, lasty2, lastx3, lasty3;

void Task_BluetoothReceive2(void *pvParameters)
{
  int count1, count2, count3 = 0;
  double sum1, sum2, sum3 = 0;
  int flag = 1;

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
        door1 = 1;
        strip.setPixelColor(0, strip.Color(0, 250, 0)); // 绿色 RGB(0,50,0)
        strip.show();
      }
    }

    SerialPort.print(door1);
    SerialPort.print(",");
    SerialPort.print(isuwb);
    SerialPort.print(",");
    SerialPort.print(distan0);
    SerialPort.print(",");
    SerialPort.print(alert_flag);
    SerialPort.print(",");
    SerialPort.print(progress);
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
    SerialPort.print(",");
    SerialPort.print(data.v1, 2);
    SerialPort.print(",");
    SerialPort.print(data.v2, 2);
    SerialPort.print(",");
    SerialPort.print(data.v3, 2);
    SerialPort.println(";");
    if (alert_flag == 1 || alert_flag == 3)
    {
      alert_flag = 0;
    }
    // flag_convert = 0;

    Serial.printf("X1=%.2fm,Y1=%.2fm,Mwave1=%.2fm,  X2=%.2fm,Y2=%.2fm,Mwave2=%.2fm,  X3=%.2fm,Y3=%.2f,Mwave3=%.2fm, progress=%d%, qf_uwb=%d, isuwb1=%d, uwb_flag=%.2fm,uwb=%.2fm, uwb0=%.2fm, isuwb=%d, 差1=%.2fm, 差2=%.2fm, 差3=%.2fm\n",
                  a1, b1, distance1,
                  a2, b2, distance2,
                  a3, b3, distance3,
                  progress, qf_uwb,
                  isuwb1, uwb_dis,
                  distan0, di0, isuwb,
                  abs(distance1 - dis0),
                  abs(distance2 - dis0),
                  abs(distance3 - dis0));
    Serial.println();
    Serial.print("v1: ");
    Serial.print(data.v1);
    Serial.print("  v2: ");
    Serial.print(data.v2);
    Serial.print("  v3: ");
    Serial.println(data.v3);
    vTaskDelay(200 / portTICK_PERIOD_MS); // 1秒发送一次
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
        // vTaskDelay(10 / portTICK_PERIOD_MS); //
        BLEAddress address(slaveMacs[i]);
        connectToSlave(i, address);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // 1秒发送一次
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
volatile int mpu_flag = 1;
void Task_BluetoothReceive3(void *pvParameters)
{
  // #ifdef INITIATOR
  while (1)
  {
    vTaskDelay(3500 / portTICK_PERIOD_MS); // 1秒发送一次
    mpu_flag = 1;
  }
}
volatile double last_uwb_dis = 0;
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
    kalAx = kalAx * g;
    kalAy = kalAy * g;
    kalAz = kalAz * g;
    aax = (int)(kalAx * 100) / 100.0; // 截取两位小数
    aay = (int)(kalAy * 100) / 100.0; // 截取两位小数
    aaz = (int)(kalAz * 100) / 100.0; // 截取两位小数

    if (abs(aax) > 0.05 || abs(aay) > 0.05 || abs(aaz) > 0.05)
    {
      // 优先检测更严重的形变情况（car_change == 1）
      if (car_change == 1 && mpu_flag == 1)
      {
        alert_flag = 3;
        if (door1 == 1)
        {
          door1 = door1 - 1;
          strip.setPixelColor(0, strip.Color(250, 0, 0)); // 绿色 RGB(0,50,0)
          strip.show();
          Serial.println("车辆严重碰撞（发生形变）!!!");
        }
        // vTaskDelay(9000 / portTICK_PERIOD_MS); // 10ms间隔
        mpu_flag = 0;
        car_change = 0;
      }
      // 如果没有形变，再检测普通擦碰
      else if (car_change == 0 && mpu_flag == 1)
      {
        alert_flag = 1;
        mpu_flag = 0;
        Serial.println("车辆轻微擦碰（未形变）!!!");
      }
      vTaskDelay(80 / portTICK_PERIOD_MS); // 10ms间隔
    }
  }
}
int id_flag = 0;
bool door_fl = 0;
volatile double v1, v2, v3;

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
      if (door1 == 1)
      {
        door1 = door1 - 1;
        id = 0;
        strip.setPixelColor(0, strip.Color(250, 0, 0)); // 绿色 RGB(0,50,0)
        strip.show();
      }
      else if (door1 == 0)
      {
        door1 = door1 + 1;
        id = 0;
        strip.setPixelColor(0, strip.Color(0, 250, 0)); // 绿色 RGB(0,50,0)
        strip.show();
      }
    }
    //&& (abs(distance1 - di0) <= 0.6 || abs(distance2 - di0) <= 0.6 || abs(distance3 - di0) <= 0.6)
    if (flag == 0 && distan0 >= 0 && distan0 <= 2.2 && isuwb == 1)
    {
      strip.setPixelColor(0, strip.Color(0, 250, 0)); // 绿色 RGB(0,50,0)
      strip.show();
      door1 = 1;
      flag = 1;
    }
    //&& (abs(distance1 - di0) <= 0.6 || abs(distance2 - di0) <= 0.6 || abs(distance3 - di0) <= 0.6)
    else if (flag == 1 && distan0 >= 3.6 && isuwb == 1)
    {
      strip.setPixelColor(0, strip.Color(250, 0, 0)); // 红色 RGB(50,0,0)
      strip.show();
      door1 = 0; // 发送数据到 LED
      flag = 0;
    }
    if (qf_uwb == 2)
    {
      uwb_dis = distance0;
      isuwb1 = is_uwb;
    }
    else if (qf_uwb == 1)
    {
      di0 = dis0;
      distan0 = distance0;
      isuwb = is_uwb;
    }
    if (car_change == 0)
    {
      car_change = detectDistanceChange(uwb_dis, last_uwb_dis);
    }
    last_uwb_dis = uwb_dis;

    Cartesian rotated1 = rotateDirect(a1, b1, 90.0);
    Cartesian rotated2 = rotateDirect(a2, b2, -90.0);
    data.x1 = rotated1.x + 0.6;
    data.y1 = rotated1.y;
    data.x2 = rotated2.x - 0.6;
    data.y2 = rotated2.y;
    data.x3 = a3;
    data.y3 = b3 + 0.6;
    Pedestrian peds[3] = {
        {data.x1, data.y1, lastx1, lasty1, 0, 0, 0.0},
        {data.x2, data.y2, lastx2, lasty2, 0, 0, 0.0},
        {data.x3, data.y3, lastx3, lasty3, 0, 0, 0.0}};
    Pedestrian peds1[3] = {
        {data.x1, data.y1, lastx1, lasty1, 0, 0, v1},
        {data.x2, data.y2, lastx2, lasty2, 0, 0, v2},
        {data.x3, data.y3, lastx3, lasty3, 0, 0, v3}};
    for (int i = 0; i < 3; i++)
    {
      update_speed(&peds1[i], 0.1);
      data.v1 = peds1[0].v;
      data.v2 = peds1[1].v;
      data.v3 = peds1[2].v;
      // Serial.printf("Ped %d 速度 = %.3f m/s\n", i + 1, peds1[i].v);
    }

    lastx1 = data.x1;
    lasty1 = data.y1;
    lastx2 = data.x2;
    lasty2 = data.y2;
    lastx3 = data.x3;
    lasty3 = data.y3;

    if (isuwb == 0)
    {
      distan0 = 0;
      di0 = 0;
    }
    // 初始化 prev_distance 和 confidence
    for (int i = 0; i < 3; ++i)
    {
      double d = calc_distance(peds[i].x, peds[i].y);
      peds[i].prev_distance = d;
      peds[i].confidence = distance_to_confidence(d);
    }

    // 模拟更新一帧
    for (int i = 0; i < 3; ++i)
    {
      peds[i].confidence = update_confidence(&peds[i]);
    }

    int nearest_index = find_nearest_index(peds, 3);
    progress = peds[nearest_index].confidence;

    vTaskDelay(50 / portTICK_PERIOD_MS); // 90ms间隔

    // Serial.print("distance0= ");
    // Serial.println(distance0);

    // Serial.print("dis0= ");
    // Serial.println(dis0);
  }
}
void setup()
{
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  // pinMode(4, OUTPUT);
  // digitalWrite(4, LOW);



  Serial.begin(115200);
  SerialPort.begin(115200, SERIAL_8N1, 16, 17); // 初始化串口
  spiBegin(UWB_IRQ, UWB_RST);
  spiSelect(UWB_SS);
  Serial.println("123");
  delay(2);
  start_uwb();
  Serial.println("456");
  delay(2);
  mpu_init(); // 初始化陀螺仪
  delay(2);
  strip.begin();
  strip.setPixelColor(0, strip.Color(200, 0, 0)); // 绿色 RGB(0,50,0)
  strip.show();                                   // 初始化所有像素为关灯状态
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
      2,                // 优先级
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
  xTaskCreatePinnedToCore(
      Task_BluetoothReceive3, // 任务函数
      "BluetoothReceive3",    // 任务名称
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
