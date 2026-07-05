#include <pico/time.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "platform_hw.h"

//--------------------------------------------------------------------+
// Low-level SPI helpers — each call is a self-contained CS frame
//--------------------------------------------------------------------+
void st7789_write_cmd(uint8_t cmd) {
  DISP_CS_SELECT();
  DISP_DC_CMD();
  spi_write_blocking(DISP_SPI, &cmd, 1);
  DISP_CS_DESELECT();
}

void st7789_write_data(uint8_t data) {
  DISP_CS_SELECT();
  DISP_DC_DATA();
  spi_write_blocking(DISP_SPI, &data, 1);
  DISP_CS_DESELECT();
}

void st7789_write_data_buf(const uint8_t *data, size_t len) {
  if (len == 0) return;

  DISP_CS_SELECT();
  DISP_DC_DATA();
  spi_write_blocking(DISP_SPI, data, len);
  DISP_CS_DESELECT();
}

//--------------------------------------------------------------------+
// Set window for partial update (CASET + RASET + RAMWR)
// Each register gets one CS frame for efficiency.
//--------------------------------------------------------------------+
void st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  uint16_t gy1 = y1 + DISP_Y_OFFSET;
  uint16_t gy2 = y2 + DISP_Y_OFFSET;
  uint8_t  buf[4];

  //--- CASET (column address) ---
  DISP_CS_SELECT();
  DISP_DC_CMD();
  spi_write_blocking(DISP_SPI, (uint8_t[]){0x2A}, 1);
  DISP_DC_DATA();
  buf[0] = x1 >> 8; buf[1] = x1 & 0xFF;
  buf[2] = x2 >> 8; buf[3] = x2 & 0xFF;
  spi_write_blocking(DISP_SPI, buf, 4);
  DISP_CS_DESELECT();

  //--- RASET (row address with Y offset) ---
  DISP_CS_SELECT();
  DISP_DC_CMD();
  spi_write_blocking(DISP_SPI, (uint8_t[]){0x2B}, 1);
  DISP_DC_DATA();
  buf[0] = gy1 >> 8; buf[1] = gy1 & 0xFF;
  buf[2] = gy2 >> 8; buf[3] = gy2 & 0xFF;
  spi_write_blocking(DISP_SPI, buf, 4);
  DISP_CS_DESELECT();

  //--- RAMWR (memory write) ---
  DISP_CS_SELECT();
  DISP_DC_CMD();
  spi_write_blocking(DISP_SPI, (uint8_t[]){0x2C}, 1);
  DISP_CS_DESELECT();
}

//--------------------------------------------------------------------+
// Write raw bytes to ST7789 (data line) — one CS frame
//--------------------------------------------------------------------+
void st7789_write_pixels(const uint8_t *data, size_t byte_count) {
  if (byte_count == 0) return;

  DISP_CS_SELECT();
  DISP_DC_DATA();
  spi_write_blocking(DISP_SPI, data, byte_count);
  DISP_CS_DESELECT();
}

//--------------------------------------------------------------------+
// RGB888 (3 B/pixel) → ST7789 RGB666 packed (3 B/pixel)
// Truncates 8 → 6 bits — no interpolation, no fake precision
//--------------------------------------------------------------------+
void st7789_pack_rgb666(const uint8_t *rgb888, uint8_t *dst, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    uint8_t r8 = rgb888[i * 3];
    uint8_t g8 = rgb888[i * 3 + 1];
    uint8_t b8 = rgb888[i * 3 + 2];

    uint8_t r6 = r8 >> 2;
    uint8_t g6 = g8 >> 2;
    uint8_t b6 = b8 >> 2;

    // ST7789 3-byte packed: [R5..R0 G5 G4] [G3..G0 B5..B2] [B1 B0 000000]
    dst[i * 3]     = (r6 << 2) | (g6 >> 4);
    dst[i * 3 + 1] = ((g6 & 0x0F) << 4) | (b6 >> 2);
    dst[i * 3 + 2] = (b6 & 0x03) << 6;
  }
}

void st7789_init(void) {

  // Reset ST7789
  gpio_put(DISP_RST_PIN, 1); // de-assert first
  sleep_ms(10);
  gpio_put(DISP_RST_PIN, 0); // assert reset
  sleep_ms(10);
  gpio_put(DISP_RST_PIN, 1); // release reset
  sleep_ms(120);             // wait for display to boot

  // Turn on back light
  gpio_put(DISP_BL_PIN, 1); // backlight on

  //=== Sleep out ===
  st7789_write_cmd(0x11);
  sleep_ms(120);

  //=== Interface configuration ===

  // MADCTL
  st7789_write_cmd(0x36);
  st7789_write_data(0x00);

  // Pixel format: 16-bit (RGB 5-6-5) — see docs/rgb565-vs-rgb666.md for rationale
  st7789_write_cmd(0x3A);
  st7789_write_data(0x55);

  //=== Frame rate & timing ===

  // Porch control
  st7789_write_cmd(0xB2);
  st7789_write_data(0x0C);
  st7789_write_data(0x0C);
  st7789_write_data(0x00);
  st7789_write_data(0x33);
  st7789_write_data(0x33);

  // Gate control
  st7789_write_cmd(0xB7);
  st7789_write_data(0x35);

  //=== Power & voltage ===

  // VCOM = 1.35V
  st7789_write_cmd(0xBB);
  st7789_write_data(0x32);

  // VDV / VRH enable
  st7789_write_cmd(0xC2);
  st7789_write_data(0x01);

  // VRH: GVDD = 4.8V
  st7789_write_cmd(0xC3);
  st7789_write_data(0x15);

  // VDV: 0V
  st7789_write_cmd(0xC4);
  st7789_write_data(0x20);

  // Frame rate: 60Hz
  st7789_write_cmd(0xC6);
  st7789_write_data(0x0F);

  // Power control
  st7789_write_cmd(0xD0);
  st7789_write_data(0xA4);
  st7789_write_data(0xA1);

  //=== Gamma (module-specific, from known-working reference) ===

  st7789_write_cmd(0xE0);
  {
    static const uint8_t pg[] = {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31,
                                 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34};
    st7789_write_data_buf(pg, sizeof(pg));
  }

  st7789_write_cmd(0xE1);
  {
    static const uint8_t ng[] = {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15, 0x31,
                                 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34};
    st7789_write_data_buf(ng, sizeof(ng));
  }

  //=== Display output ===

  // Inversion on
  st7789_write_cmd(0x21);

  // Display on
  st7789_write_cmd(0x29);
}

//--------------------------------------------------------------------+
// Initialize display hardware: SPI1, DC, RST, and backlight GPIOs
//--------------------------------------------------------------------+
void platform_init(void) {

  // --- SPI1: mode 0, MSB first, 8-bit data ---
  spi_init(DISP_SPI, DISP_SPI_BAUD);
  spi_set_format(DISP_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

  gpio_set_function(DISP_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(DISP_TX_PIN, GPIO_FUNC_SPI);

  // CS — software-controlled GPIO (not hardware SPI)
  gpio_init(DISP_CS_PIN);
  gpio_set_dir(DISP_CS_PIN, GPIO_OUT);
  gpio_put(DISP_CS_PIN, 1); // deselected (active-low)

  // --- DC (Data/Command control) ---
  gpio_init(DISP_DC_PIN);
  gpio_set_dir(DISP_DC_PIN, GPIO_OUT);
  gpio_put(DISP_DC_PIN, 0);

  // --- RST (hardware reset, active-low) ---
  gpio_init(DISP_RST_PIN);
  gpio_set_dir(DISP_RST_PIN, GPIO_OUT);

  // --- BL (backlight, active-high) ---
  gpio_init(DISP_BL_PIN);
  gpio_set_dir(DISP_BL_PIN, GPIO_OUT);
}
