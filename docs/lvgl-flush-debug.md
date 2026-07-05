# LVGL Flush Callback Debug / LVGL 刷新回调排错记录

## Symptom / 现象

Screen shows scrambled pixels (花屏 / 雪花) when LVGL renders.

## Root Cause / 根因

**Byte count mismatch in `st7789_write_pixels` call.**

```c
// ❌ Wrong — passes pixel count, but function expects byte count
st7789_write_pixels(px_map, w * h);

// ✅ Correct — RGB565 = 2 bytes per pixel
st7789_write_pixels(px_map, w * h * 2);
```

`st7789_write_pixels(const uint8_t *data, size_t byte_count)` treats the second argument as **raw byte count**. Passing `w * h` (pixel count) sends only **half** the pixel data when using RGB565 (2 B/px). The ST7789 reads the expected bytes for the window but only receives half — the remaining pixel positions are filled with whatever electrical noise is on the SPI bus or residual FIFO data, producing the scrambled display.

### Why "half data" looks like snow / 为什么"数据减半"看起来像雪花

```
LVGL renders:  [P0][P1][P2][P3]...[P99]     (100 pixels = 200 bytes)
SPI sends:     [P0][P1]...[P49]              (100 bytes = 50 pixels only)
ST7789 expects: 200 bytes for the window
ST7789 receives: 100 bytes → 50 pixels
Remaining 50 pixels: undefined (FIFO garbage / bus noise) → random colours
```

## Checklist / 排查清单

| # | Check | Common Failure |
|---|-------|----------------|
| 1 | `st7789_write_pixels` byte count | `w*h` instead of `w*h*2` for RGB565 |
| 2 | `lv_display_create(w, h)` matches panel | wrong resolution → partial rendering |
| 3 | `st7789_set_window` Y offset | missing `DISP_Y_OFFSET` → data at wrong rows |
| 4 | LVGL buffer size `sizeof(buf1)` | too small → constant partial redraw, tearing |
| 5 | `vTaskDelay` in render loop | 1000 ms → 1 fps, looks frozen even if frame is correct |
| 6 | `LV_COLOR_DEPTH` matches `COLMOD` | 24-bit LVGL + 16-bit ST7789 → colour corruption |

### Typical healthy flush callback / 正常的回调模板

```c
static void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    st7789_set_window(area->x1, area->y1, area->x2, area->y2);
    st7789_write_pixels(px_map, w * h * 2);   // RGB565: 2 bytes per pixel
    lv_display_flush_ready(disp);
}

// In the task loop:
while (1) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));   // 5 ms ≈ 200 Hz tick
}
```

## Fix Applied / 修正内容

| File | Change |
|------|--------|
| `src/app_disp_task.c` | `w * h` → `w * h * 2` |
| `src/app_disp_task.c` | `pdMS_TO_TICKS(1000)` → `pdMS_TO_TICKS(5)` |
