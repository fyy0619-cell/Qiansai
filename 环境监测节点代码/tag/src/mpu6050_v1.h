#include "I2Cdev.h"
#include "Wire.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include <Kalman.h>
MPU6050 mpu;
class SimpleKalmanFilter
{
private:
    double Q_angle;   // 过程噪声协方差
    double R_measure; // 测量噪声协方差
    double angle;     // 滤波后的状态估计
    double P;         // 状态协方差矩阵
    double K;         // 卡尔曼增益

public:
    // 构造函数
    SimpleKalmanFilter(double Q_angle, double R_measure)
    {
        this->Q_angle = Q_angle;
        this->R_measure = R_measure;
        this->angle = 0;
        this->P = 1;
        this->K = 0;
    }

    // 更新卡尔曼滤波器状态
    double update(double measurement)
    {
        // 预测步骤
        P = P + Q_angle;

        // 更新步骤
        K = P / (P + R_measure);                   // 计算卡尔曼增益
        angle = angle + K * (measurement - angle); // 更新状态估计
        P = (1 - K) * P;                           // 更新状态协方差

        return angle; // 返回滤波后的值
    }
};

// 创建卡尔曼滤波器对象
SimpleKalmanFilter kalmanX(0.001, 0.1); // 过程噪声和测量噪声参数
// MPU control/status vars
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer 从mpu6050读取的最原始数据

// orientation/motion vars

Quaternion q;              // [w, x, y, z]         四元数
VectorInt16 aa;            // [x, y, z]            加速度计
VectorInt16 aaReal;        // [x, y, z]            无重力加速度计
VectorInt16 aaWorld;       // [x, y, z]            world-frame accel sensor measurements
VectorInt16 Gyro;          // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;       // [x, y, z]            重力加速度
float euler[3];            // [psi, theta, phi]    欧拉角
float pry[3];              // [pitch, roll, yaw]   俯仰，滚转，偏航
float apry0, apry1, apry2; // [pitch, roll, yaw]   俯仰，滚转，偏航
double lefttarget = 0;
double righttarget = 0;
int16_t ax, ay, az;
int16_t gx, gy, gz;
float arealx, arealy, arealz;
float arealx1, arealy1, arealz1;
float x, y, z;
float kalAx, kalAy, kalAz, next;
float aax, aay, aaz = 0;
int mode = 0;
// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = {'$', 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, '\r', '\n'};
// A pair of varibles to help parse serial commands (thanks Fergs)
int arg = 0;
int index_ = 0;

// Variable to hold an input character
char chr;

// Variable to hold the current single-character command
char cmd;

// Character arrays to hold the first and second arguments
char argv1[16];
char argv2[16];

// The arguments converted to integers
long arg1;
long arg2;

volatile bool mpuInterrupt = false; // indicates whether MPU interrupt pin has gone high
void dmpDataReady()
{
    mpuInterrupt = true;
}



void mpu_init()
{ // 初始化MPU6050
// join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
#endif

    Serial.begin(115200);
    while (!Serial)
        ; // wait for Leonardo enumeration, others continue immediately

    // initialize device
    Serial.println(F("Initializing I2C devices..."));
    mpu.initialize();
    // pinMode(INTERRUPT_PIN, INPUT);

    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

    // make sure it worked (returns 0 if so)
    if (devStatus == 0)
    {
        // Calibration Time: generate offsets and calibrate our MPU6050
        mpu.CalibrateAccel(6);
        mpu.CalibrateGyro(6);
        mpu.PrintActiveOffsets();
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
        // Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
        Serial.println(F(")..."));
        attachInterrupt(digitalPinToInterrupt(2), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    }
    else
    {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }
}


void mpu_get_data()
{ // 获取并处理MPU6050数据
    // read a packet from FIFO
    if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
    {
        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(pry, &q, &gravity); // 姿态
        // mpu.dmpGetEuler(euler, &q);
        // mpu.dmpGetAccel(&aa, fifoBuffer);
        // mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
        // mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
        // mpu.dmpGetGyro(&Gyro, fifoBuffer);
    }
}

void resetCommand()
{
    cmd = 0; // NULL;
    memset(argv1, 0, sizeof(argv1));
    memset(argv2, 0, sizeof(argv2));
    arg1 = 0;
    arg2 = 0;
    arg = 0;
    index_ = 0;
}

void show_q()
{ // 显示四元数
    Serial.print("Q: ");
    Serial.print(q.w);
    Serial.print("\t");
    Serial.print(q.x);
    Serial.print("\t");
    Serial.print(q.y);
    Serial.print("\t");
    Serial.println(q.z);
}

void show_euler()
{ // 显示欧拉角
    // Serial.print("euler\t");
    // Serial.print(euler[0] * 180/M_PI);
    // Serial.print("\t");
    // Serial.print(euler[1] * 180/M_PI);
    // Serial.print("\t");
    // Serial.println(euler[2] * 180/M_PI);
    Serial.printf("%.2f,%.2f,%.2f\n", euler[0] * 180 / M_PI, euler[1] * 180 / M_PI, euler[2] * 180 / M_PI);
}

void show_gravity()
{ // 显示重力加速度在三个轴线上的分量
    // Serial.print("gravity:\t");
    // Serial.print(gravity.x);
    // Serial.print("\t");
    // Serial.print(gravity.y);
    // Serial.print("\t");
    // Serial.println(gravity.z);
    Serial.printf("%.2f,%.2f,%.2f\n", (float)gravity.x / 16384 * 9.8, (float)gravity.y / 16384 * 9.8, (float)gravity.z / 16384 * 9.8);
}

void show_pry()
{ // 显示俯仰、滚转、偏航
    // Serial.print("PRY:\t\t");
    // Serial.print(pry[0]);
    // Serial.print("\t");
    // Serial.print(pry[1]);
    // Serial.print("\t");
    // Serial.println(pry[2]);
    // Serial.printf("%.2f,%.2f,%.2f", pry[0], pry[0], pry[0]);

    apry0 = pry[0] * 180 / 3.14;
    apry1 = pry[1] * 180 / 3.14;
    apry2 = pry[2] * 180 / 3.14;
    Serial.printf("%.2f ,%.2f ,%.2f\n", apry0, apry1, apry2);
}

void show_aa()
{ // 显示原始加速度（平动+重力）
    // Serial.print("aa:\t");
    // Serial.print(aa.x);
    // Serial.print("\t");
    // Serial.print(aa.y);
    // Serial.print("\t");
    // Serial.println(aa.z);
    Serial.printf("%.2f,%.2f,%.2f\n", aa.x, aa.y, aa.z);
}

void show_aaReal()
{ // 显示平动加速度（无重力）
    // Serial.print("areal\t");
    // Serial.print(aaReal.x);
    // Serial.print("\t");
    // Serial.print(aaReal.y);
    // Serial.print("\t");
    // Serial.println(aaReal.z);
    Serial.printf("%.2f,%.2f,%.2f\n", aaReal.x, aaReal.y, aaReal.z);
}

void show_aaWorld()
{
    // Serial.print("aworld\t");
    // Serial.print(aaWorld.x);
    // Serial.print("\t");
    // Serial.print(aaWorld.y);
    // Serial.print("\t");
    // Serial.println(aaWorld.z);
    Serial.printf("%.2f,%.2f,%.2f\n", aaWorld.x, aaWorld.y, aaWorld.z);
}

void show_teapot()
{
    teapotPacket[2] = fifoBuffer[0];
    teapotPacket[3] = fifoBuffer[1];
    teapotPacket[4] = fifoBuffer[4];
    teapotPacket[5] = fifoBuffer[5];
    teapotPacket[6] = fifoBuffer[8];
    teapotPacket[7] = fifoBuffer[9];
    teapotPacket[8] = fifoBuffer[12];
    teapotPacket[9] = fifoBuffer[13];
    Serial.write(teapotPacket, 14);
    teapotPacket[11]++; // packetCount, loops at 0xFF on purpose
}