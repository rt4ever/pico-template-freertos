#include "display_test.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/time.h"
#include "platform_hw.h"

//--------------------------------------------------------------------+
// Shared row buffer — all test patterns fill this, one row at a time
//--------------------------------------------------------------------+
static uint8_t disp_row[DISP_HOR_RES * 2];   // 480 bytes, global

//--------------------------------------------------------------------+
// Generic full-screen blit — calls fill(y, row) per line, sends via SPI
//--------------------------------------------------------------------+
typedef void (*fill_row_fn)(uint16_t y, uint8_t *row);

static void disp_flush_fullscreen(fill_row_fn fill) {
  const uint16_t cols = DISP_HOR_RES;
  const uint16_t rows = DISP_VER_RES;
  const uint16_t y0   = DISP_Y_OFFSET;

  // Window = full visible area
  st7789_write_cmd(0x2A);
  st7789_write_data(0x00);
  st7789_write_data(0x00);
  st7789_write_data((cols - 1) >> 8);
  st7789_write_data((cols - 1) & 0xFF);

  st7789_write_cmd(0x2B);
  st7789_write_data(0x00);
  st7789_write_data(y0);
  st7789_write_data(((y0 + rows - 1) >> 8) & 0xFF);
  st7789_write_data((y0 + rows - 1) & 0xFF);

  st7789_write_cmd(0x2C);

  for (uint16_t y = 0; y < rows; y++) {
    fill(y, disp_row);
    st7789_write_data_buf(disp_row, cols * 2);
  }
}

//--------------------------------------------------------------------+
// Checkerboard pattern — 20 px tiles
//--------------------------------------------------------------------+
static void checkerboard_fill(uint16_t y, uint8_t *row) {
  const uint8_t  tile = 20;
  const uint16_t cols = DISP_HOR_RES;

  for (uint16_t x = 0; x < cols; x++) {
    uint16_t c = ((x / tile) ^ (y / tile)) & 1 ? 0x0000 : 0xFFFF;
    row[x * 2]     = c >> 8;
    row[x * 2 + 1] = c & 0xFF;
  }
}

void display_test_checkerboard(void) {
  disp_flush_fullscreen(checkerboard_fill);
}

//--------------------------------------------------------------------+
// Random colour card — 20 px tiles, matches checkerboard grid
//--------------------------------------------------------------------+
#define CCTILE      20

static uint32_t cc_seed;

static uint16_t cc_tile_colour(uint8_t tx, uint8_t ty) {
  // SplitMix64-inspired hash → high-saturation RGB565
  uint32_t h = cc_seed ^ ((uint32_t)tx << 16) ^ ((uint32_t)ty << 8);
  h = (h ^ (h >> 16)) * 0x45D9F3B;
  h = (h ^ (h >> 16)) * 0x45D9F3B;
  h = h ^ (h >> 16);

  // Ensure minimum brightness for visual punch
  uint8_t r = (h >> 3)  & 0x1F;
  uint8_t g = (h >> 11) & 0x3F;
  uint8_t b = (h >> 19) & 0x1F;
  if (r < 4) r = 4;    // avoid near-black tiles
  if (g < 6) g = 6;
  if (b < 4) b = 4;

  return (r << 11) | (g << 5) | b;
}

static void colorcard_fill(uint16_t y, uint8_t *row) {
  const uint16_t cols = DISP_HOR_RES;
  uint8_t  ty = y / CCTILE;

  for (uint16_t x = 0; x < cols; x++) {
    uint8_t  tx = x / CCTILE;
    uint16_t c  = cc_tile_colour(tx, ty);
    row[x * 2]     = c >> 8;
    row[x * 2 + 1] = c & 0xFF;
  }
}

void display_test_colorcard(void) {
  cc_seed = to_ms_since_boot(get_absolute_time());   // new palette each call
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
