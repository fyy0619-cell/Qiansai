// #include <Arduino.h>
// #include <SPI.h>

// // ICM45686寄存器地址
// #define ICM45686_REG_WHOAMI 0x00
// #define ICM45686_REG_PWR_MGMT_1 0x06
// #define ICM45686_REG_GYRO_XOUT_H 0x1D
// #define ICM45686_REG_ACCEL_XOUT_H 0x23

// // SPI片选引脚
// const int CS_PIN = 5;

// // 陀螺仪和加速度计灵敏度
// const float GYRO_SENSITIVITY = 131.0;
// const float ACCEL_SENSITIVITY = 16384.0;

// // 姿态角
// float roll = 0.0;
// float pitch = 0.0;
// float yaw = 0.0;

// // 三轴加速度
// float ax = 0.0;
// float ay = 0.0;
// float az = 0.0;

// // 读取ICM45686寄存器
// byte readRegister(byte reg) {
//   digitalWrite(CS_PIN, LOW);
//   SPI.transfer(reg | 0x80);
//   byte data = SPI.transfer(0x00);
//   digitalWrite(CS_PIN, HIGH);
//   return data;
// }

// // 写入ICM45686寄存器
// void writeRegister(byte reg, byte value) {
//   digitalWrite(CS_PIN, LOW);
//   SPI.transfer(reg);
//   SPI.transfer(value);
//   digitalWrite(CS_PIN, HIGH);
// }

// // 初始化ICM45686
// void initICM45686() {
//   // 检查设备ID
//   byte whoami = readRegister(ICM45686_REG_WHOAMI);
//   if (whoami != 0x41) {
//     Serial.println("ICM45686 not found!");
//     while (1);
//   }

//   // 唤醒设备
//   writeRegister(ICM45686_REG_PWR_MGMT_1, 0x01);
// }

// // 读取陀螺仪数据
// void readGyroData(float &gx, float &gy, float &gz) {
//   int16_t gx_raw = (readRegister(ICM45686_REG_GYRO_XOUT_H) << 8) | readRegister(ICM45686_REG_GYRO_XOUT_H + 1);
//   int16_t gy_raw = (readRegister(ICM45686_REG_GYRO_XOUT_H + 2) << 8) | readRegister(ICM45686_REG_GYRO_XOUT_H + 3);
//   int16_t gz_raw = (readRegister(ICM45686_REG_GYRO_XOUT_H + 4) << 8) | readRegister(ICM45686_REG_GYRO_XOUT_H + 5);

//   gx = (float)gx_raw / GYRO_SENSITIVITY;
//   gy = (float)gy_raw / GYRO_SENSITIVITY;
//   gz = (float)gz_raw / GYRO_SENSITIVITY;
// }

// // 读取加速度计数据
// void readAccelData(float &ax, float &ay, float &az) {
//   int16_t ax_raw = (readRegister(ICM45686_REG_ACCEL_XOUT_H) << 8) | readRegister(ICM45686_REG_ACCEL_XOUT_H + 1);
//   int16_t ay_raw = (readRegister(ICM45686_REG_ACCEL_XOUT_H + 2) << 8) | readRegister(ICM45686_REG_ACCEL_XOUT_H + 3);
//   int16_t az_raw = (readRegister(ICM45686_REG_ACCEL_XOUT_H + 4) << 8) | readRegister(ICM45686_REG_ACCEL_XOUT_H + 5);

//   ax = (float)ax_raw / ACCEL_SENSITIVITY;
//   ay = (float)ay_raw / ACCEL_SENSITIVITY;
//   az = (float)az_raw / ACCEL_SENSITIVITY;
// }

// // 解算姿态角
// void calculateAttitude(float gx, float gy, float gz, float ax, float ay, float az, float dt) {
//   // 加速度计计算初始姿态角
//   float accel_roll = atan2(ay, az) * 180 / PI;
//   float accel_pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180 / PI;

//   // 陀螺仪积分更新姿态角
//   roll += gx * dt;
//   pitch += gy * dt;
//   yaw += gz * dt;

//   // 互补滤波融合加速度计和陀螺仪数据
//   roll = 0.98 * roll + 0.02 * accel_roll;
//   pitch = 0.98 * pitch + 0.02 * accel_pitch;
// }

// // 去除重力影响
// void removeGravity(float ax, float ay, float az, float roll, float pitch, float &ax_filt, float &ay_filt, float &az_filt) {
//   float gx = sin(pitch);
//   float gy = -cos(pitch) * sin(roll);
//   float gz = -cos(pitch) * cos(roll);

//   ax_filt = ax - gx;
//   ay_filt = ay - gy;
//   az_filt = az - gz;
// }

// void setup() {
//   Serial.begin(115200);
//   SPI.begin();
//   pinMode(CS_PIN, OUTPUT);
//   digitalWrite(CS_PIN, HIGH);

//   initICM45686();
// }

// void loop() {
//   static unsigned long last_time = millis();
//   unsigned long current_time = millis();
//   float dt = (current_time - last_time) / 1000.0;
//   last_time = current_time;

//   float gx, gy, gz;
//   readGyroData(gx, gy, gz);

//   readAccelData(ax, ay, az);

//   calculateAttitude(gx, gy, gz, ax, ay, az, dt);

//   float ax_filt, ay_filt, az_filt;
//   removeGravity(ax, ay, az, roll, pitch, ax_filt, ay_filt, az_filt);

//   Serial.print("Roll: ");
//   Serial.print(roll);
//   Serial.print(" Pitch: ");
//   Serial.print(pitch);
//   Serial.print(" Yaw: ");
//   Serial.print(yaw);
//   Serial.print(" Ax: ");
//   Serial.print(ax_filt);
//   Serial.print(" Ay: ");
//   Serial.print(ay_filt);
//   Serial.print(" Az: ");
//   Serial.println(az_filt);

//   delay(10);
// }    