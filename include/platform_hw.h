#ifndef PLATFORM_HW_H
#define PLATFORM_HW_H

#include <stdint.h>
#include <stddef.h>

//--------------------------------------------------------------------+
// ST7789V2 Display Pin Definitions (3-Wire SPI on SPI1)
//--------------------------------------------------------------------+
#define DISP_SPI        spi1

#define DISP_SCK_PIN    10
#define DISP_TX_PIN     11
#define DISP_CS_PIN     13

#define DISP_DC_PIN     12
#define DISP_RST_PIN    14
#define DISP_BL_PIN     6

#define DISP_SPI_BAUD   (40 * 1000 * 1000)  // 40 MHz

// CS is software-controlled via GPIO, not hardware SPI peripheral.
// Callers must toggle DISP_CS_PIN manually around SPI transactions.
#define DISP_CS_SELECT()    gpio_put(DISP_CS_PIN, 0)
#define DISP_CS_DESELECT()  gpio_put(DISP_CS_PIN, 1)

// DC pin helpers
#define DISP_DC_CMD()       gpio_put(DISP_DC_PIN, 0)
#define DISP_DC_DATA()      gpio_put(DISP_DC_PIN, 1)

// Reset pin helpers
#define DISP_RST_RESET()      gpio_put(DISP_RST_PIN, 0)
#define DISP_RST_SET()        gpio_put(DISP_RST_PIN, 1)

//--------------------------------------------------------------------+
// Display Parameters
//--------------------------------------------------------------------+
#define DISP_HOR_RES        240
#define DISP_VER_RES        280
#define DISP_Y_OFFSET       20    // rows 0-19 are invisible porch — visible area starts at row 20

//--------------------------------------------------------------------+
// API
//--------------------------------------------------------------------+
void platform_init(void);
void st7789_init(void);

// Low-level SPI helpers (also usable by LVGL flush callback)
void st7789_write_cmd(uint8_t cmd);
void st7789_write_data(uint8_t data);
void st7789_write_data_buf(const uint8_t *data, size_t len);

#endif /* PLATFORM_HW_H */
