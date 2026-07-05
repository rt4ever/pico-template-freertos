# RGB565 vs RGB666 — Why 16-bit is the Right Choice / 为什么选 16-bit

## Bandwidth & Memory / 带宽与内存

| Parameter | RGB565 (16-bit) | RGB666 (18-bit) | Delta |
|-----------|----------------|-----------------|-------|
| Bytes per pixel | 2 | 3 | **+50%** |
| Full-screen frame (240×280) | 134,400 B | 201,600 B | **+67,200 B** |
| SPI transfer time @ 40 MHz | ~27 ms | ~40 ms | **+48%** |
| Row buffer (240 px) | 480 B | 720 B | **+240 B** |
| LVGL partial buffer (320×10 px) | 6,400 B | 9,600 B | **+3,200 B** |

Every frame costs 50% more SPI bandwidth and RAM for colour precision that the human eye cannot resolve on a small TFT.

## Perceptual Limits / 人眼感知极限

### Colour discrimination

```
RGB565:  32 red × 64 green × 32 blue  = 65,536 colours
RGB666:  64 red × 64 green × 64 blue  = 262,144 colours
```

The human eye can discriminate roughly **1–2 million colours** under optimal conditions (large colour patches, controlled lighting). On a **2.0–2.8 inch TFT at arm's length**, the just-noticeable-difference (JND) is far coarser:

- **Small pixel pitch** (~0.18 mm) means adjacent colour differences are averaged by the eye
- **Viewing distance** reduces effective angular resolution
- **Ambient light** washes out the low-order bits regardless of bit depth
- **LCD panel gamma non-linearity** dominates over bit-depth colour error

> **Empirical rule**: Going beyond 16-bit colour on displays under 4 inches provides no visible benefit in 95% of embedded use cases.

### Gradient banding / 渐变色条带

The only scenario where 16-bit depth can show **visible banding** is smooth colour gradients (e.g. a full-screen blue-to-black radial gradient). In practice:

- Embedded UIs rarely use gradients
- ST7789's internal gamma LUT already smooths the response curve
- Dithering (if needed) can be applied in software to simulate higher depth at zero bandwidth cost

## Where 18-bit Actually Matters / 18-bit 的真正用途

262K colour mode is useful in these scenarios:

1. **Photo/image display** — JPEG/PNG viewer with smooth tonal transitions
2. **Medical/industrial displays** — where gray-level discrimination is safety-critical
3. **Video playback** — motion smooths out banding, but 24-bit source → 16-bit truncation creates subtle artifacts
4. **Large panels (>5 inch)** — where pixel pitch is larger and colour differences are perceptible

None of these apply to a typical MCU-driven TFT used for widgets/gauges/menus.

## Decision / 决策

| | RGB565 | RGB666 |
|--|--------|--------|
| SPI bandwidth | Baseline | +50% |
| RAM | Baseline | +50% |
| Visual quality | Indistinguishable for UI | Marginally better for gradients |
| LVGL compatibility | Native (`LV_COLOR_DEPTH 16`) | Requires conversion (`LV_COLOR_DEPTH 24`) |
| **Verdict** | ✅ **Use this** | ⚠️ Keep `st7789_pack_rgb666` for future |

`COLMOD = 0x05` (RGB565) is the correct default. The `st7789_pack_rgb666` helper is retained in `platform_hw.c` and can be enabled by changing `COLMOD` to `0x06` and switching `LV_COLOR_DEPTH` to 24 if a specific use case requires it.
