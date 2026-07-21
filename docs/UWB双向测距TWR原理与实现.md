# UWB 双向测距（TWR）原理与实现

> 本文档讲解系统中 **DW3000 UWB 测距** 的原理与工程实现，覆盖：为什么用 TWR、四步握手时序、飞行时间/距离公式推导、多节点（U1–U6）组网，以及与源码 `环境监测节点代码/tag/src/uwb.cpp` 的逐段对照。

---

## 一、为什么用 UWB + 双向测距

- **UWB（Ultra-Wideband，超宽带）** 用纳秒级窄脉冲，时间分辨率极高，测距精度可达 **10cm 级**，抗多径、穿透性好，适合室内/近场定位。
- 测距本质是「测电磁波飞行时间（Time of Flight, ToF）」再乘光速。难点在于：**收发两端时钟不同步**，无法直接用「单程时间」。
- **TWR（Two-Way Ranging，双向测距）** 通过「一来一回」用**同一个设备的本地时钟**测往返时间，从而**消除两端时钟偏差**，无需时钟同步。本系统用其增强版 **DS-TWR（对称双边双向测距）**，进一步抵消时钟漂移，精度更高。

---

## 二、DS-TWR 四步握手

发起者（tag，`initiator`）与应答者（锚点，`responder`）交换 4 帧，每次收发都由 DW3000 硬件打上纳秒级时间戳：

```
     tag (initiator)                              锚点 (responder)
        │                                              │
   t0   │ ───────────────  Poll  ───────────────────▶ │ t1   (poll_rx)
        │                                              │
   t4   │ ◀──────────────  Ack   ─────────────────────│ t2   (ack_tx, 回传 t1)
        │                                              │
   t5   │ ───────────────  Range ───────────────────▶ │ t3   (range_rx)
        │                                              │
   t8   │ ◀──────────────  Final ─────────────────────│      (回传 t2, t3)
        │                                              │
        │  收集 6 个时间戳，计算 ToF → 距离             │
```

四个关键时间量：

| 量 | 含义 | 代码变量 |
|---|---|---|
| `t_round_1` | tag 发 Poll → 收 Ack 的往返 | `ack_rx_ts − poll_tx_ts` |
| `t_reply_1` | 锚点 收 Poll → 发 Ack 的回复延时 | `ack_tx_ts − poll_rx_ts` |
| `t_round_2` | 锚点 发 Ack → 收 Range 的往返 | `range_rx_ts − ack_tx_ts` |
| `t_reply_2` | tag 收 Ack → 发 Range 的回复延时 | `range_tx_ts − ack_rx_ts` |

---

## 三、飞行时间与距离公式

DS-TWR 的飞行时间估计（抵消时钟漂移的对称形式）：

```
        t_round_1 · t_round_2 − t_reply_1 · t_reply_2
ToF  =  ─────────────────────────────────────────────
        t_round_1 + t_round_2 + t_reply_1 + t_reply_2
```

距离：

```
distance = ToF × c        （c = 光速 ≈ 3×10⁸ m/s）
```

对应 `uwb.cpp` 的 `initiator()`：

```c
t_round_1 = ack_rx_ts[i] - poll_tx_ts;
t_round_2 = (range_rx_ts - ack_tx_ts);
t_reply_1 = (ack_tx_ts - poll_rx_ts[i]);
t_reply_2 = range_tx_ts - ack_rx_ts[i];
tof = ((t_round_1*t_round_2 - t_reply_1*t_reply_2)
      / (t_round_1 + t_round_2 + t_reply_1 + t_reply_2) + 33) * DWT_TIME_UNITS;
distance = tof * SPEED_OF_LIGHT - 0.57;   // 天线/常数校正
```

- `DWT_TIME_UNITS`：把 DW3000 时间戳计数换算成秒。
- `+33` 与 `−0.57`：针对本套天线的**经验校正**（天线延时、走线等），换硬件需重新标定。
- **天线延时** `TX_ANT_DLY / RX_ANT_DLY = 16385`（`uwb.h`）在初始化时写入 DW3000（`dwt_settxantennadelay` / `dwt_setrxantennadelay`），补偿信号在天线到芯片间的固定延迟。

---

## 四、稳健性处理（工程细节）

源码在裸公式外做了多重保护：

1. **多次平均**：每 **10 次**测距求均值再输出（`count==10`），抑制随机抖动。
2. **合法性门限**：均值落在 `[0, 20]m` 外判为野值，沿用上一次有效值 `lastsum`。
3. **偏置补偿**：对外距离 `distance0 = 均值 + 0.6`，`dis0 = 均值`。
4. **超时复位**：`initiator()` 收不到应答（`SYS_STATUS_ALL_RX_TO`）→ 置 `is_uwb=0`（车主离开范围）并发 `FUNC_CODE_RESET` 复位链路。
5. **身份校验**：帧源 ID（`MSG_SID_IDX`）与目标序列 `target_uids[]` 不符则丢弃，防串扰。
6. **RX 超时** `RX_TIMEOUT_UUS = 400000`，避免无限等待卡死状态机。

---

## 五、多节点组网（U1–U6）

系统支持 **2~6 个 UWB 节点**协同，节点身份与目标由编译宏与运行期配置共同决定：

| 宏 | 身份 | UID | 角色 |
|---|---|---|---|
| `MAIN_U1` | U1 | 14 | **发起者** `INITIATOR`（本 tag 工程默认）|
| `MAIN_U2` … `MAIN_U6` | U2–U6 | 18/22/26/30/34 | 应答者 `responder` |

- 编译时用 `#define MAIN_Ux` 选身份（`uwb.cpp` 顶部 / `platformio.ini` build_flags）。
- `set_target_uids()` 按 `NUM_NODES` 用 `switch` 贯穿（fall-through）填充每个节点应测的目标序列，实现链式测距调度。
- `initiator()` 负责「发 Poll → 收全部 Ack → 发 Range → 收全部 Final → 算距离」；`responder()` 负责转发 poll/ack/range 并回 final。两者共用一套 `wait_poll/ack/range/final` 状态标志与 `counter` 推进状态机。

---

## 六、与 DW3000 驱动的关系

底层收发、时间戳、寄存器操作由 Qorvo/Decawave 官方驱动 `dw3000_*` 提供，业务层只调用其 API：

| 功能 | 驱动 API |
|---|---|
| 初始化 / 配置 | `dwt_initialise` `dwt_configure` `dwt_configuretxrf` |
| 天线延时 | `dwt_settxantennadelay` `dwt_setrxantennadelay` |
| 发送 | `dwt_writetxdata` `dwt_writetxfctrl` `dwt_starttx` |
| 接收 | `dwt_rxenable` `dwt_readrxdata` |
| 时间戳 | `get_tx_timestamp_u64` `get_rx_timestamp_u64` |
| 时间戳打包 | `resp_msg_set_ts` `resp_msg_get_ts` |

SPI 引脚在 `dw3000_port.cpp`：`SCK=12 / MISO=13 / MOSI=11`；片选/复位/中断在 `uwb.h`：`SS=14 / RST=7 / IRQ=5`。

---

## 七、在整机中的作用

UWB 测得的「车主与车距离」`distan0` + 有效标志 `is_uwb`，是**无感开关门**的核心依据：

```
distan0 ∈ [0, 2.2]m 且 is_uwb==1  → 开门（绿灯，door1=1）
distan0 ≥ 3.6m       且 is_uwb==1  → 关门（红灯，door1=0）
```

此外 UWB 帧还承载**按键开关门指令**（`key_flag`）与**距离突变**信息（辅助 `Task_MPU6050_data` 判断严重碰撞）。完整逻辑见 [tag 节点 README](../环境监测节点代码/tag/README.md)。
