# ST7789V2 Initialization Sequence / ST7789V2 初始化流程解析

> Based on ST7789V2 datasheet v1.2 (Sitronix). Each register write is annotated with its datasheet section and purpose.

---

## Phase 0: Hardware Reset / 硬件复位

```
RST pin: HIGH → LOW (10 ms) → HIGH (120 ms wait)
BL pin: HIGH (backlight on immediately, so we can see output as soon as it appears)
```

| Step | Register / Pin | Value | Description |
|------|---------------|-------|-------------|
| RST=0 | `DISP_RST_PIN` (GP14) | LOW × 10 ms | 拉低复位引脚，所有寄存器回到默认值 |
| RST=1 | `DISP_RST_PIN` (GP14) | HIGH + 120 ms | 释放复位，等待内部 LDO / 振荡器稳定 |
| BL=1 | `DISP_BL_PIN` (GP6) | HIGH | 背光先开——后续初始化过程中即可观察画面变化 |

> **Datasheet §8.1**: After HW reset, VDD must be stable and the internal power-on sequence takes ~120 ms before the first command can be accepted.

---

## Phase 1: Exit Sleep Mode / 退出睡眠

| Cmd | Name | Data | Description |
|-----|------|------|-------------|
| `0x11` | **SLPOUT** (Sleep Out) | — | 唤醒面板供电电路，启动内部 DC/DC 升压、VCOM 发生器、振荡器。这是所有显示操作的前提。 |

```
st7789_write_cmd(0x11);   // SLPOUT, datasheet §8.12
sleep_ms(120);            // 等待内部电源稳定
//This command turn off sleep mode. 
//-In this mode the DC/DC converter is enable, internal display oscillator is started, and panel scanning is started. 
```

> **Datasheet §8.12**: Sleep Out 后需等 120 ms，供电电路完全建立后才能发后续命令。

---

## Phase 2: Interface Configuration / 接口配置

### 2.1 MADCTL `0x36` — 内存访问控制 / Memory Data Access Control

```
0x36 → 0x00
MADCTL (36h): Memory Data Access Control 

```

| Bit | Name | Value | Meaning |
|-----|------|-------|---------|
| 7 | **MY** | 0 | 行地址从上到下递增（不翻转） |
| 6 | **MX** | 0 | 列地址从左到右递增（不翻转） |
| 5 | **MV** | 0 | 不交换行/列方向 |
| 4 | **ML** | 0 | 垂直刷新方向正常 |
| 3 | **RGB-BGR** | 0 | RGB 顺序（非 BGR） |
| 2 | **MH** | 0 | 水平刷新方向正常 |
| 1:0 | — | 00 | 保留 |

> **Datasheet §8.4.28**: 控制扫描方向和像素颜色顺序。`0x00` = 最常规的默认方向，适合竖屏使用。若需要横屏，调整 MY/MX/MV 组合。

### 2.2 COLMOD `0x3A` — 像素格式 / Interface Pixel Format

```
0x3A → 0x05
```

| Value | Meaning |
|-------|---------|
| `0x05` | RGB 5-6-5 (16-bit/pixel, 65K colors) |
| `0x06` | RGB 6-6-6 (18-bit/pixel) |
| `0x07` | RGB 6-6-6 (24-bit, unused bits padded) |

> **Datasheet §8.4.25**: 选 RGB565 与 LVGL `LV_COLOR_DEPTH 16` 匹配，每个像素 2 字节。此寄存器应在 Sleep Out 之后设置。

---

## Phase 3: Frame Rate & Timing / 帧率和时序控制

### 3.1 PORCTRL `0xB2` — Porch Control

```
0xB2 → 0x0C, 0x0C, 0x00, 0x33, 0x33
```

| Byte | Name | Purpose |
|------|------|---------|
| 1 | **BP** (Back Porch) = 12 | 行同步后沿（像素时钟数），保证行间稳定 |
| 2 | **FP** (Front Porch) = 12 | 行同步前沿 |
| 3 | **T_VBP** = 0 | 垂直后沿 |
| 4 | **VFP** = 51 | 垂直前沿（行数），防止画面顶部/底部失真 |
| 5 | **VBP** = 51 | 垂直后沿 |

> **Datasheet §8.7.2**: 空白区间（porch）用于行/场回扫。参数不当会导致画面偏移或撕裂。此模组规格为 240×320 物理面板，可见区 280 行从 Y=20 开始——这些 porch 参数与该物理尺寸匹配。

### 3.2 GCTRL `0xB7` — Gate Control

```
0xB7 → 0x35
```

| Bits | Name | Value | Purpose |
|------|------|-------|---------|
| 7:5 | **VGHS** | 3 (~16V) | Gate high level — TFT 栅极开启电压 |
| 4:0 | **VGLS** | 5 (~-10V) | Gate low level — TFT 栅极关断电压 |

> **Datasheet §8.7.4**: 控制 TFT 扫描线的栅极驱动电压。VGHS 必须足够高以完全打开 TFT 开关；VGLS 必须足够低以完全关闭。不匹配会导致对比度不足或漏光。

### 3.3 FRCTRL2 `0xC6` — Frame Rate Control (Normal Mode)

```
0xC6 → 0x0F
```

| Bits | Name | Value | Purpose |
|------|------|-------|---------|
| 4:0 | **RTNA** | 15 (60 Hz) | 正常模式刷新率 |

> **Datasheet §8.7.7**: 控制面板刷新率。0x0F 对应~60 Hz（默认值）。

---

## Phase 4: Power & Voltage / 电源和电压

### 4.1 VCOMS `0xBB` — VCOM Setting

```
0xBB → 0x32 (~1.35 V)
```

> **Datasheet §8.7.20**: **VCOM 是 LCD 公共电极的参考电压**。液晶像素靠像素电极与 VCOM 之间的交流电压差来偏转。VCOM 值是面板厂校准的关键参数——**每块模组不同**。0x32 来自已知可用的参考初始化，偏差 0.1 V 即可导致全屏无显示。

### 4.2 VDVVRHEN `0xC2` — VDV/VRH Command Enable

```
0xC2 → 0x01
```

| Bit | Name | Value | Meaning |
|-----|------|-------|---------|
| 0 | **CMDEN** | 1 | 允许写 VDV (`0xC4`) 和 VRH (`0xC3`) 寄存器 |

> **Datasheet §8.7.22**: 安全锁。GD32 等部分 MCU 写保护，需先使能才能配置后续电压寄存器。

### 4.3 VRHS `0xC3` — VRH Set

```
0xC3 → 0x15 (GVDD ≈ 4.8 V)
```

| Value | Voltage |
|-------|---------|
| 0x00 | 3.20 V |
| 0x15 | ~4.80 V |
| 0x3F | 6.40 V |

> **Datasheet §8.7.23**: **GVDD 是 Gamma 参考电压源**。GVDD 决定 Gamma 校正曲线的动态范围。过高→功耗增加、液晶可能过驱；过低→颜色发灰、对比度下降。0x15 (4.8 V) 是该模组的校准值。

### 4.4 VDVS `0xC4` — VDV Set

```
0xC4 → 0x20 (0 V offset)
```

> **Datasheet §8.7.24**: VDV 是 Gamma 曲线偏移电压。0x20 表示零偏移（基准点）。

### 4.5 PWCTRL1 `0xD0` — Power Control 1

```
0xD0 → 0xA4, 0xA1
```

| Byte | Bits | Purpose |
|------|------|---------|
| 1 | [7:4] **VRHP** | 字符电压泵调节 |
| 1 | [3:0] **BT** | 升压因子调节 |
| 2 | [7:4] **VHPS** | 高压泵调节 |
| 2 | [3:0] **VAP** | 模拟电压调节 |

> **Datasheet §8.7.29**: 控制内部 DC/DC 电源泵。驱动 TFT 面板需要高于逻辑电压的模拟电压（~10-16V），由片内电荷泵产生。参数影响升压效率和输出纹波。

---

## Phase 5: Gamma Calibration / Gamma 校正

### 5.1 PVGAMCTRL `0xE0` — Positive Gamma

```
0xE0 → 14 bytes of gamma lookup table
PG: [D0,08,0E,09,09,05,31,33,48,17,14,15,31,34]
```

### 5.2 NVGAMCTRL `0xE1` — Negative Gamma

```
0xE1 → 14 bytes of gamma lookup table
NG: [D0,08,0E,09,09,15,31,33,48,17,14,15,31,34]
```

> **Datasheet §8.7.38-39**: **Gamma 校正表将数字灰度值 (0-63) 映射到模拟驱动电压**。
>
> - **PG (Positive Gamma)**: 控制像素电压高于 VCOM 的灰度值
> - **NG (Negative Gamma)**: 控制像素电压低于 VCOM 的灰度值
>
> LCD 需要交流驱动防止液晶分子电偶极矩累积（直流会导致永久损坏）。正负 Gamma 必须对称，否则画面闪烁（flicker）。
>
> **这 14×2=28 个值是面板厂针对特定模组校准的，不可通用**。偏移几个灰度级会导致偏色甚至无显示。

---

## Phase 6: Display Output / 显示输出

### 6.1 INVON `0x21` — Display Inversion On

```
0x21
```

> **Datasheet §8.12.9**: 启用**点反转**（dot inversion）驱动——相邻像素极性交替变化。反转是 LCD AC 驱动的基本要求：
> - 不反转 → 液晶偏压累积 → 直流残留 → 永久烧屏（image sticking）
> - 点反转 → 空间上相邻像素正负交替 → 视觉平均为零 → 无闪烁

### 6.2 DISPON `0x29` — Display On

```
0x29
```

> **Datasheet §8.12.15**: 开启面板显示输出。在此命令之前，GRAM 写入是有效的，但面板不显示。此命令之后，面板持续扫描 GRAM 内容输出到屏幕。

---

## Command Sequence Summary / 命令流程总结

```
┌─────────────────────────────────────────────┐
│ 0. RST → BL (硬件复位 + 背光)                │  启动时序
├─────────────────────────────────────────────┤
│ 1. SLPOUT (0x11)   退出睡眠                  │  电源就绪
│    wait 120 ms                               │
├─────────────────────────────────────────────┤
│ 2. MADCTL (0x36)   扫描方向 + RGB 顺序       │  接口配置
│ 3. COLMOD (0x3A)   像素格式 RGB565           │
├─────────────────────────────────────────────┤
│ 4. PORCTRL (0xB2)  前后沿时序                │  显示时序
│ 5. GCTRL (0xB7)    栅极驱动电压              │
│ 6. FRCTRL2 (0xC6)  刷新率 60Hz               │
├─────────────────────────────────────────────┤
│ 7. VCOMS (0xBB)    VCOM 参考电压             │  电压校准
│ 8. VDVVRHEN (0xC2) 电压寄存器使能            │
│ 9. VRHS (0xC3)     Gamma 参考电压 (GVDD)     │
│10. VDVS (0xC4)     Gamma 偏移电压            │
│11. PWCTRL1 (0xD0)  电荷泵功率控制            │
├─────────────────────────────────────────────┤
│12. PVGAMCTRL (0xE0) 正 Gamma 校正曲线        │  Gamma LUT
│13. NVGAMCTRL (0xE1) 负 Gamma 校正曲线        │
├─────────────────────────────────────────────┤
│14. INVON (0x21)    点反转启动                 │  显示输出
│15. DISPON (0x29)   开启显示                   │
└─────────────────────────────────────────────┘
```

### Critical Calibration Values / 关键校准值

| Register | Value | Parameter | Calibrated By |
|----------|-------|-----------|---------------|
| `0xBB` VCOMS | `0x32` | VCOM ≈ 1.35 V | Panel vendor |
| `0xC3` VRHS | `0x15` | GVDD ≈ 4.80 V | Panel vendor |
| `0xE0` PGAMMA | 14 bytes | Positive gamma LUT | Panel vendor |
| `0xE1` NGAMMA | 14 bytes | Negative gamma LUT | Panel vendor |
| `0xB2` PORCTRL | 5 bytes | Porch timing | Module design |
| `DISP_Y_OFFSET` | 20 | Visible row start | Module design |

> **这些值不是通用的！** 每个 ST7789 模组有自己的 VCOM / Gamma / Porch 校准。使用错误的校准值是导致"初始化成功但无显示"的首要原因（见 `docs/hardware.md` 中的调试记录）。
