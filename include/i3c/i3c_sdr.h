#ifndef I3C_SDR_H
#define I3C_SDR_H

#include <stddef.h>
#include <stdint.h>

#include "bitbang_i3c.h"

void i3c_sdr_init(void);

bool i3c_sdr_transfer(uint8_t addr, const uint8_t *tx, size_t tx_len,
                      uint8_t *rx, size_t rx_len);

bool i3c_reg_read8(uint8_t dev, uint8_t reg, uint8_t *data);
bool i3c_reg_write8(uint8_t dev, uint8_t reg, uint8_t *data);

bool i3c_reg_read16(uint8_t dev, uint8_t reg, uint16_t *data);
bool i3c_reg_write16(uint8_t dev, uint8_t reg, uint16_t *data);

bool i3c_reg_read32(uint8_t dev, uint8_t reg, uint32_t *data);
bool i3c_reg_write32(uint8_t dev, uint8_t reg, uint32_t *data);

bool i3c_reg_read(uint8_t dev, uint8_t reg, uint8_t *data, uint8_t len);
bool i3c_reg_write(uint8_t dev, uint8_t reg, uint8_t *data, uint8_t len);

#endif