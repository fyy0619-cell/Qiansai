# tag —— 车主随身 UWB 标签 / 车端 UWB 主控（ESP32-S3）

> 本节点是「基于多模态感知的智能汽车无感交互与安防系统」中的 **UWB 测距与无感交互核心**。
> 它以 **ESP32-S3** 为主控，集成 **DW3000 UWB 双向测距、MPU6050 擦碰检测、WS2812B 状态灯、BLE 雷达坐标汇聚、UART2 上传树莓派**，
> 通过 FreeRTOS 多任务并行运行，实现「靠近开门 / 远离关门 / 按键开关门 / 擦碰预警」。

本工程即 `E:\嵌赛\TAG` 的 ESP32-S3 版本，是旧 `esp32dev` 版 tag 节点的升级：主控换成 ESP32-S3、新增 WS2812B 灯驱动与 Kalman 滤波擦碰检测，UWB 测距、BLE 汇聚、串口上传三条链路合一。

---

## 一、一句话定位

> **车主随身携带的「电子钥匙」**：用 UWB 与车身实时测距（厘米级），判断车主与车的远近来无感开关门；同时把毫米波雷达的行人坐标、擦碰状态一起打包上传给树莓派 Qt 界面。

---

## 二、硬件平台与引脚接线

| 项目 | 值 | 说明 |
|---|---|---|
| 主控 | **ESP32-S3-DevKitC-1** | 16MB Flash，`qio_opi` 内存模式 |
| 框架 | Arduino + PlatformIO | `platform = espressif32` |
| 调试串口 | 115200 | `Serial`，USB |

### 引脚表（均来自源码实测，非示意）

| 外设 | 外设引脚 | ESP32-S3 GPIO | 总线 / 说明 | 出处 |
|---|---|---|---|---|
| **DW3000 UWB** | SCK | **12** | SPI 时钟 | `dw3000_port.cpp` `SPI_SCK` |
| | MISO | **13** | SPI | `dw3000_port.cpp` `SPI_MISO` |
| | MOSI | **11** | SPI | `dw3000_port.cpp` `SPI_MOSI` |
| | CS / SS | **14** | SPI 片选 | `uwb.h` `UWB_SS` |
| | RST | **7** | 复位 | `uwb.h` `UWB_RST` |
| | IRQ | **5** | 中断输入 | `uwb.h` `UWB_IRQ` |
| **WS2812B 灯带** | DIN | **10** | 3 颗全彩灯（`LED_COUNT 3`）；上电时 GPIO10 先被拉高 | `ws2812b.h` `LED_PIN` |
| **MPU6050 IMU** | SDA / SCL | I2C 默认（Wire）| 400kHz，地址 `0x68` | `mpu6050_v1.h` `Wire.begin()` |
| | INT（DMP 中断）| **2** | `attachInterrupt(digitalPinToInterrupt(2), …)` | `mpu6050_v1.h` |
| **UART2 → 树莓派** | TX | **17** | → 树莓派 RX | `start.cpp` `SerialPort.begin(115200,…,16,17)` |
| | RX | **16** | ← 树莓派 TX（收人脸开门指令）| 同上 |
| **BLE（板载）** | — | — | 作为 **Client**，连雷达链末端（MAC `68:25:dd:f0:59:32`）| `start.cpp` `slaveMacs[]` |
| 电源 | 5V / GND | — | 与外设**共 GND** | — |

> ⚠️ DW3000、MPU6050 为 3.3V 逻辑，注意电平匹配；MAC 地址与 UUID 均为硬编码，换板需回填。

---

## 三、整体架构（数据流）

```
                 ┌──────────────────────── tag (ESP32-S3) ────────────────────────┐
   毫米波雷达链   │                                                                 │
 (BLE1→2→3) ─BLE─▶│  notifyCallback ── RadarPacket 解析 ─▶ 行人坐标 a1..b3           │
                 │        │                                    │                    │
                 │        ▼                                    ▼                    │
 车身另一枚      │   坐标旋转/平移(rotateDirect)          置信度/速度估计            │
 DW3000 锚点 ─UWB─▶│   DS-TWR 测距(initiator) ─▶ distan0    (progress / v1..v3)      │──UART2──▶ 树莓派 Qt
                 │        │                                    │                115200│   (显示+调度)
                 │        ▼                                    ▼                    │
                 │   开关门决策(距离阈值/按键) ─▶ door1     MPU6050 擦碰(alert_flag) │
                 │        │                                    │                    │
                 │        ▼                                    ▼                    │
                 │   WS2812B 状态灯(绿=开/红=关/碰撞)      擦碰置 alert_flag=1/3     │
                 └─────────────────────────────────────────────────────────────────┘
```

**一句话**：UWB 测车主与车的距离 → 阈值判断开关门 → 点亮状态灯并把「门状态 + 行人坐标 + 擦碰标志」经 UART2 发给树莓派。

---

## 四、软件模块分解

代码分为 **自写业务层**（`start.cpp` / `uwb.*` / `mpu6050_v1.h` / `ws2812b.*`）与 **第三方驱动层**（`dw3000_*` / `MPU6050*` / `I2Cdev*`）。业务层是阅读重点。

### 4.1 `start.cpp` —— 应用入口与 FreeRTOS 任务编排

`setup()` 依次初始化：GPIO10 拉高 → 调试串口 → UART2(16/17) → UWB SPI(`spiBegin/spiSelect`) → `start_uwb()` → `mpu_init()` → WS2812 → BLE 并连接雷达从机，随后创建 6 个固定核心任务：

| 任务 | 核心 | 优先级 | 栈 | 职责 |
|---|---|---|---|---|
| `Task_DataProcess` | 1 | 2 | 16384 | 死循环跑 `initiator()`，即 **UWB DS-TWR 测距主循环** |
| `Task_BluetoothReceive` | 0 | 1 | 4096 | 监测 BLE 连接，断线自动重连雷达从机 |
| `Task_BluetoothReceive1` | 0 | 1 | 4096 | **开关门决策**：距离阈值 / 按键 / UWB 有效性；坐标旋转、行人置信度与速度 |
| `Task_BluetoothReceive2` | 0 | 1 | 4096 | **UART2 上行/下行**：打包发送状态帧；接收树莓派人脸开门指令 |
| `Task_BluetoothReceive3` | 0 | 1 | 4096 | 每 3.5s 复位 `mpu_flag`，给擦碰检测「去抖窗口」 |
| `Task_MPU6050_data` | 0 | 1 | 4096 | 读 DMP + Kalman，判断擦碰/形变，置 `alert_flag` |

> 设计要点：把耗时、时序敏感的 **UWB 测距独占核心 1**，其余感知/通信任务放核心 0，避免测距被打断。

### 4.2 `uwb.cpp` / `uwb.h` —— DW3000 双向测距（DS-TWR）

采用 **对称双边双向测距（Symmetric Double-Sided Two-Way Ranging, DS-TWR）**，四步握手：

```
tag(initiator)                     锚点(responder)
   │  ── Poll ───────────────────▶ │  记录 poll_rx
   │  ◀────────────── Ack ──────── │  回 Ack(带 poll_rx 时间戳)
   │  ── Range ──────────────────▶ │  记录 range_rx
   │  ◀───────────── Final ─────── │  回 Final(带 ack_tx / range_rx 时间戳)
   │  用 4 个往返时间算 ToF → 距离   │
```

飞行时间与距离（`initiator()` 内）：

```
tof = (t_round_1·t_round_2 − t_reply_1·t_reply_2)
      / (t_round_1 + t_round_2 + t_reply_1 + t_reply_2) · DWT_TIME_UNITS
distance = tof · SPEED_OF_LIGHT − 0.57   // 天线/常数校正
```

- 每 **10 次测距取均值**，并做 `[0, 20]m` 合法性门限滤除野值，得到 `distance0`（= 均值 + 0.6 偏置）。
- 超时置 `is_uwb = 0`（车主不在范围），并发 `FUNC_CODE_RESET` 复位链路。
- `qf_uwb` 区分两路距离上报（1 / 2），`key_flag`（帧内 `rx_buffer[0][27]`）承载按键开关门指令。

**多节点角色**（`uwb.h`）：`U1~U6` 对应 UID `14/18/22/26/30/34`。编译时用 `MAIN_U1`…`MAIN_U6` 选身份，`MAIN_U1` 同时定义 `INITIATOR`（发起者）。`set_target_uids()` 按节点数配置测距目标序列，支持 2~6 节点组网。本工程默认 **`MAIN_U1` + `INITIATOR`**。

### 4.3 `mpu6050_v1.h` —— IMU 擦碰检测（DMP + Kalman）

- `mpu_init()`：初始化 DMP，写入陀螺/加速度校准偏置，标定后开 DMP，DMP 中断挂 GPIO2。
- `Task_MPU6050_data`：读四元数→重力→**去重力线加速度** `aaReal`，经内置 `SimpleKalmanFilter`（`Q=0.001, R=0.1`）滤波并换算成 `m/s²`。
- 阈值判定：任一轴 `|a| > 0.05` 视为受力；
  - `car_change==1`（UWB 距离突变佐证严重形变）→ `alert_flag = 3`（严重碰撞），若门开则自动关门 + 红灯；
  - 否则 → `alert_flag = 1`（轻微擦碰）。
- `mpu_flag` + `Task_BluetoothReceive3` 构成 **3.5s 去抖**，避免一次碰撞重复报警。

### 4.4 `ws2812b.cpp` / `ws2812b.h` —— 状态指示灯

`Adafruit_NeoPixel strip(3, GPIO10, NEO_GRB+NEO_KHZ800)`。约定：

| 颜色 | 含义 |
|---|---|
| 🟢 绿 `(0,250,0)` | 开门 / 车主授权靠近 |
| 🔴 红 `(250,0,0)` | 关门 / 车辆碰撞 |
| 🟠 启动 `(200,0,0)` | 上电初始化 |

### 4.5 BLE 客户端 —— 汇聚毫米波雷达坐标

作为 Client 连接雷达链末端（`SERVICE_UUID / CHARACTERISTIC_UUID`），`notifyCallback` 解析 `RadarPacket_t`（`id + 3 组 x/y + dis`，**大端转小端**），换算成米级坐标 `a1/b1 … a3/b3`。`Task_BluetoothReceive1` 内用 `rotateDirect()` 做坐标旋转+平移，将各雷达局部坐标统一到 **车体正中心为原点** 的车体坐标系，并估计行人**速度 v** 与**置信度 progress**（越近置信度越高，`<0.75m` 记为越线）。

### 4.6 UART2 通信协议（与树莓派）

**上行**（tag → 树莓派，`Task_BluetoothReceive2`，约 200ms 一帧，逗号分隔、`;` 结尾）：

```
door1, isuwb, distan0, alert_flag, progress, x1,y1, x2,y2, x3,y3, v1,v2,v3 ;
```

| 字段 | 含义 |
|---|---|
| `door1` | 门状态（1 开 / 0 关）|
| `isuwb` | UWB 是否有效（车主是否在范围）|
| `distan0` | 车主与车距离（m）|
| `alert_flag` | 擦碰标志（0 无 / 1 轻微 / 3 严重）|
| `progress` | 最近行人置信度（越线预警进度）|
| `x1..y3` | 3 个行人相对车体坐标（m）|
| `v1..v3` | 3 个行人速度（m/s）|

**下行**（树莓派 → tag）：收到字节 `1` → 人脸识别通过，直接开门（绿灯 + `door1=1`）。

---

## 五、无感开关门逻辑（`Task_BluetoothReceive1`）

| 触发 | 条件 | 动作 |
|---|---|---|
| 靠近开门 | `distan0 ∈ [0, 2.2]m` 且 `isuwb==1` 且当前为关 | 绿灯，`door1=1` |
| 远离关门 | `distan0 ≥ 3.6m` 且 `isuwb==1` 且当前为开 | 红灯，`door1=0` |
| 按键开关门 | UWB 帧携带 `key_flag/id==1` | 翻转门状态 |
| 人脸开门 | UART2 收到 `1` | 绿灯开门 |
| 碰撞自动关门 | `alert_flag==3` 且门开 | 红灯，`door1=0` |

> 距离带滞回（开 2.2m / 关 3.6m）避免临界抖动反复开关门。

---

## 六、编译与烧录（PlatformIO）

```ini
[env:esp32s3_wroom]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
upload_speed = 921600
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit NeoPixel@^1.15.5
    tkjelectronics/Kalman Filter Library@^1.0.2
    plerup/EspSoftwareSerial@^8.2.0
```

```bash
# 在本目录（环境监测节点代码/tag/）
pio run                 # 编译
pio run -t upload       # 烧录
pio device monitor      # 115200 串口调试
```

**选择 UWB 身份**：在 `uwb.cpp` 顶部用 `#define MAIN_U1`…`MAIN_U6` 指定（默认 `MAIN_U1` = 发起者）。**回填 MAC**：把雷达链末端板子的实际 MAC 填到 `start.cpp` 的 `slaveMacs[]`。

---

## 七、与系统其他节点的关系

```
BLE1/2/3(雷达) ──BLE菊花链──▶ tag(本节点) ──UART2──▶ 树莓派 Qt
          车身另一枚 DW3000 ◀──UWB DS-TWR──▶ tag
```

- **上游**：3 个毫米波雷达节点（`../BLE1` `../BLE2` `../BLE3`）经 BLE 级联把行人坐标汇聚过来。
- **对端**：车身侧的 DW3000（见 `../anchor`）与本节点做 UWB 测距。
- **下游**：树莓派（`../../qt`、`../../camera`）接收状态帧做界面显示、录像与人脸识别，并回发开门指令。
- **旁路**：擦碰事件可经 `../thingscloud` 上云触发微信预警。

整体框架见仓库根 [README](../../README.md) 与 [docs/硬件架构与接线说明](../../docs/硬件架构与接线说明.md)、[docs/UWB双向测距(TWR)原理与实现](../../docs/UWB双向测距TWR原理与实现.md)。

---

## 八、目录 / 文件说明

| 文件 | 类型 | 说明 |
|---|---|---|
| `src/start.cpp` | **自写** | 应用入口、FreeRTOS 任务、BLE 汇聚、开关门决策、UART 协议 |
| `src/uwb.cpp` `src/uwb.h` | **自写** | DW3000 DS-TWR 测距状态机、U1~U6 组网 |
| `src/mpu6050_v1.h` | **自写** | MPU6050 DMP 封装 + Kalman + 擦碰检测 |
| `src/ws2812b.cpp` `src/ws2812b.h` | **自写** | WS2812B 状态灯 |
| `src/main1.cpp` | 示例 | FreeRTOS 队列生产者/消费者示例（整体注释，非运行代码）|
| `src/dw3000_*` | 第三方 | Qorvo/Decawave DW3000 官方驱动库 |
| `src/MPU6050*` `src/I2Cdev*` `src/helper_3dmath.h` | 第三方 | jrowberg I2Cdevlib（MPU6050 + DMP）|
| `platformio.ini` | 配置 | 目标板、依赖库 |

---

## 九、已知点 / 注意事项

- **硬编码依赖**：BLE 从机 MAC、Service/Characteristic UUID、UWB 天线延时 `TX/RX_ANT_DLY=16385`、距离偏置常数（`+0.6` / `−0.57`）均为本套硬件实测值，换硬件需重新标定。
- **GPIO10 复用**：`setup()` 先将 GPIO10 拉高，随后 `strip.begin()` 又把它作为 WS2812 数据脚，实际由灯驱动占用。
- **距离偏置**：`distance0 = 均值 + 0.6`，`distance = tof·c − 0.57`，是针对本天线的经验校正，非通用值。
- **`main1.cpp` 为示例**：整体被注释，仅作 FreeRTOS 队列参考，不参与编译逻辑。
- 构建产物 `.pio/`、IDE 配置 `.vscode/` 已由 `.gitignore` 排除。
