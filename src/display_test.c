#include "display_test.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/time.h"
#include "platform_hw.h"

//--------------------------------------------------------------------+
// Shared row buffer — RGB565 (2 B/px)
//--------------------------------------------------------------------+
static uint8_t disp_row[DISP_HOR_RES * 2];   // 480 bytes

//--------------------------------------------------------------------+
// Generic full-screen blit
//   fill(y, row)  — writes RGB565 pixels (2 B/px)
//--------------------------------------------------------------------+
typedef void (*fill_row_fn)(uint16_t y, uint8_t *row);

static void disp_flush_fullscreen(fill_row_fn fill) {
  const uint16_t cols = DISP_HOR_RES;
  const uint16_t rows = DISP_VER_RES;

  st7789_set_window(0, 0, cols - 1, rows - 1);

  for (uint16_t y = 0; y < rows; y++) {
    fill(y, disp_row);
    st7789_write_pixels(disp_row, cols * 2);
  }
}

//--------------------------------------------------------------------+
// Checkerboard pattern — 20 px tiles
//--------------------------------------------------------------------+
static void checkerboard_fill(uint16_t y, uint8_t *row) {
  const uint8_t  tile = 20;
  const uint16_t cols = DISP_HOR_RES;

  for (uint16_t x = 0; x < cols; x++) {
    bool black = ((x / tile) ^ (y / tile)) & 1;
    uint16_t c = black ? 0x0000 : 0xFFFF;
    row[x * 2]     = c >> 8;
    row[x * 2 + 1] = c & 0xFF;
  }
}

void display_test_checkerboard(void) {
  disp_flush_fullscreen(checkerboard_fill);
}

//--------------------------------------------------------------------+
// Random colour card — 20 px tiles, 24-bit source → RGB565 output
//--------------------------------------------------------------------+
#define CCTILE      20

static uint32_t cc_seed;

static void colorcard_fill(uint16_t y, uint8_t *row) {
  const uint16_t cols = DISP_HOR_RES;
  uint8_t  ty = y / CCTILE;

  for (uint16_t x = 0; x < cols; x++) {
    uint8_t tx = x / CCTILE;

    // Hash tile coords + seed → 24-bit colour (8-bit/channel)
    uint32_t h = cc_seed ^ ((uint32_t)tx << 16) ^ ((uint32_t)ty << 8);
    h = (h ^ (h >> 16)) * 0x45D9F3B;
    h = (h ^ (h >> 16)) * 0x45D9F3B;
    h = h ^ (h >> 16);

    uint8_t r8 = (h >> 16) & 0xFF;
    uint8_t g8 = (h >> 8)  & 0xFF;
    uint8_t b8 =  h        & 0xFF;

    // Minimum brightness
    if (r8 < 32) r8 = 32;
    if (g8 < 32) g8 = 32;
    if (b8 < 32) b8 = 32;

    // Pack to RGB565
    uint16_t c = ((uint16_t)(r8 >> 3) << 11)
               | ((uint16_t)(g8 >> 2) << 5)
               |  (uint16_t)(b8 >> 3);
    row[x * 2]     = c >> 8;
    row[x * 2 + 1] = c & 0xFF;
  }
}

void display_test_colorcard(void) {
  cc_seed = to_ms_since_boot(get_absolute_time());
  disp_flush_fullscreen(colorcard_fill);
}

//--------------------------------------------------------------------+
// Toggle loop — checkerboard <-> colour card every second
//--------------------------------------------------------------------+
void display_test_run(void) {
  for (;;) {
    display_test_checkerboard();
    vTaskDelay(pdMS_TO_TICKS(1000));
    display_test_colorcard();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
