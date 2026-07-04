#include <pico/time.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "platform_hw.h"

//--------------------------------------------------------------------+
// DMA channel for SPI TX (claimed once in platform_init)
//--------------------------------------------------------------------+
static int spi_dma_chan = -1;

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

  if (spi_dma_chan >= 0 && len > 16) {
    // DMA path — worthwhile for bulk transfers
    dma_channel_config cfg = dma_channel_get_default_config(spi_dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, DREQ_SPI1_TX);

    dma_channel_configure(
        spi_dma_chan, &cfg,
        &spi_get_hw(DISP_SPI)->dr,   // write to SPI data register
        data,                          // read from buffer
        len,                           // byte count
        false);                        // don't start yet

    dma_channel_start(spi_dma_chan);
    dma_channel_wait_for_finish_blocking(spi_dma_chan);

    // Drain TX FIFO — last bytes may still be shifting out
    while (spi_is_busy(DISP_SPI));
  } else {
    // Blocking path — single bytes or DMA unavailable
    spi_write_blocking(DISP_SPI, data, len);
  }

  DISP_CS_DESELECT();
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

  // Pixel format: 16-bit (RGB 5-6-5)
  st7789_write_cmd(0x3A);
  st7789_write_data(0x05);

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

  // --- SPI DMA channel ---
  spi_dma_chan = dma_claim_unused_channel(true);

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
