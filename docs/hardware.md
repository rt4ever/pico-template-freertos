# Hardware Configuration / 硬件配置

## LVGL Display — ST7789V2 (3-Wire SPI) / LVGL 显示屏 — ST7789V2（3 线 SPI）

| Signal / 信号 | GPIO | RP2040 SPI1 Function | Description / 描述 |
|---------------|------|-----------------------|---------------------|
| SPI-SCK       | GP10 | SPI1 SCK              | SPI clock / SPI 时钟 |
| SPI-TX        | GP11 | SPI1 TX (MOSI)        | SPI data output, 3-wire bidirectional / SPI 数据输出，3 线双向 |
| SPI-CS        | GP13 | GPIO (software)       | Chip select, active-low / 片选，低有效 |
| DC            | GP12 | GPIO                  | Data/Command control (ST7789) / 数据/命令控制 |
| RST           | GP14 | GPIO                  | Hardware reset (ST7789), active-low / 硬件复位，低有效 |
| BL            | GP6  | GPIO                  | Backlight control / 背光控制 |

### Interface Details / 接口详情

- **Chip / 芯片**: ST7789V2 (Sitronix) — 240×320 TFT LCD controller / 控制器，本模组可见区 240×280
- **SPI mode / SPI 模式**: 4 线 SPI 接口（SCK + MOSI + CS + DC，无 MISO）。市面上常称为"3 线 SPI"，指的是不含 MISO 数据回读线。The ST7789 does not send data back in this configuration.
- **SPI peripheral / SPI 外设**: Pico SPI1 (`spi1`)。使用 SPI1 而非 SPI0，避免与其他外设冲突。
- **DC pin / DC 引脚**: 控制当前 SPI 字节为命令（DC=0）还是数据（DC=1）。由 LVGL 的 ST7789 驱动通过 GPIO 控制。
- **RST pin / RST 引脚**: 低有效硬件复位。启动时拉低 10ms 再拉高，等待 120ms 后控制器就绪。
- **BL pin / BL 引脚**: 背光控制，高有效。支持 PWM 调光 (`pwm_set_gpio_level()`)。

### SPI1 Default Pins / SPI1 默认引脚 (RP2040)

```
SPI1 SCK  → GPIO 10 (default) or GPIO 14
SPI1 TX   → GPIO 11 (default) or GPIO 15
SPI1 RX   → GPIO 12 (default) or GPIO 8   ← not used in 3-wire mode / 本配置未使用
SPI1 CSn  → GPIO 13 (default) or GPIO 9
```

本配置使用 **GPIO 10/11/13** 作为 SCK/TX/CS —— 即 SPI1 的默认引脚映射。
This configuration uses **GPIO 10/11/13** for SCK/TX/CS — the default SPI1 pin mapping.

### Display Parameters / 显示参数

| Parameter / 参数 | Value / 值 | Note / 备注 |
|------------------|------------|--------------|
| `DISP_HOR_RES`   | 240        | Horizontal resolution / 水平分辨率 |
| `DISP_VER_RES`   | 280        | Vertical resolution / 垂直分辨率 |
| `DISP_Y_OFFSET`  | 20         | Rows 0–19 are invisible porch / 前 20 行为不可见 porch 区 |

---

## ST7789V2 Init Debugging Notes / ST7789V2 初始化调试记录

### Symptom / 现象

No display output at 4 MHz SPI. Register writes appeared correct but the screen remained blank.

SPI 降至 4 MHz 仍无任何显示。寄存器写入看似正常，但屏幕始终全黑。

### Root Cause Analysis / 根因分析

三个独立问题叠加，每个都足以导致无显示：

Three independent issues overlapped, each sufficient to prevent visible output:

---

#### 1. VCOM (0xBB) — 公共电极电压错误 / wrong common-electrode voltage

| Parameter / 参数 | 原值 Original | 修正 Correct |
|------------------|--------------|--------------|
| Reg 0xBB         | `0x19`       | `0x32` (~1.35 V) |

VCOM 是 LCD 面板的公共电极参考电压。液晶分子靠像素电极与 VCOM 之间的交流电压差来偏转。VCOM 不匹配会导致 AC 驱动波形整体偏移，液晶无法偏转——**整个面板呈现均匀一致的状态，看起来就是"什么都没显示"**。

VCOM is the common-electrode reference voltage. LCD pixels rotate by the AC voltage difference across the liquid-crystal cell, and VCOM is one side of that differential. A mismatched VCOM shifts the AC drive waveform so the liquid crystal never rotates — the entire panel appears uniformly blank regardless of pixel data.

---

#### 2. 0xB6 (Display Function Control) — 参考初始化未使用此寄存器 / not in the reference init

原代码向 0xB6 写入了 `0x0A, 0x82`。此寄存器控制：

The original sequence wrote `0x0A, 0x82` to register 0xB6, which controls:

- **ISM[2:0]** — 色深缩减模式 / colour-reduction mode
- **RM** — 分辨率选择 / resolution selection (240×320 vs 320×240)
- **DM[1:0]** — 显示运行模式 / display operating mode (normal / idle / partial)

已知可用的参考初始化**完全不碰此寄存器**。写入不匹配该模组的值可能将显示引擎静默配置到不可用的模式。移除该写入，让硬件保持上电默认值。

The known-working reference init does **not** touch this register at all. Writing incorrect values here can silently configure the display engine into a non-functional mode. Removing this write leaves the hardware at its safe power-on default.

---

#### 3. Y 偏移 — 数据写入了不可见行 / Y-offset — data targeted invisible rows

| Parameter / 参数      | 原值 Original | 修正 Correct |
|------------------------|--------------|--------------|
| RASET Y-start / Y 起始 | `0`          | `20` (`DISP_Y_OFFSET`) |

ST7789V2 的 GRAM 为 240×320，但本模组可见区仅 280 行。前 20 行（Y=0~19）是不可见的 porch 区，用于行时序同步。所有像素数据都被写入此隐藏区域，因此即使 VCOM 和 Gamma 正确，仍然看不到任何画面。

The ST7789V2 GRAM is 240×320, but the visible area is 280 rows. The first 20 rows (Y=0~19) are an invisible porch used for timing. All pixel data was written into this hidden region, so correct VCOM/Gamma alone would still have produced no visible result.

---

#### 4. 次要因素 / Minor contributors

| 寄存器 | 原值 Original | 修正 Correct | 说明 |
|--------|--------------|--------------|------|
| 0xC3 VRH | `0x12` | `0x15` | Gamma 参考电压 GVDD ≈ 4.8 V。偏差会导致 Gamma 曲线整体偏移。 |
| 0xE0 / 0xE1 Gamma | 14 组值不同 | 参考值 | Gamma 错误通常表现为偏色而非全黑，但叠加上 VRH 偏差后，像素驱动电压可能坍缩到同一灰度。 |

- **VRH (0xC3)**: `0x12` → `0x15`. Gamma reference voltage (GVDD ≈ 4.8 V). Off-nominal VRH shifts the entire gamma curve.
- **Gamma tables (0xE0 / 0xE1)**: All 14 values differ from the module-tested reference. Incorrect gamma normally causes colour casts rather than a blank screen, but combined with wrong VRH the pixel drive voltages can collapse to the same gray level.

---

### Key Takeaway / 结论

> **始终从特定模组的已知可用的初始化序列开始。** 数据手册附录中的通用 ST7789 寄存器值是不够的——每个面板模组都有自己校准过的 VCOM、Gamma 和 porch 偏移参数。这其中任何一项静默失败都足以阻止面板产生可见图像。

> **Always start from a known-working init sequence for the specific module.** Generic ST7789 register dumps from datasheet application notes are not sufficient — each panel module has its own VCOM, Gamma, and porch-offset calibration. A silent failure in any one of these can prevent the panel from producing a visible image.
