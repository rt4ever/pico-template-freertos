#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "i3c_sdr.h"
#include "FreeRTOS.h"
#include "bitbang_i3c.h"

static const i3c_phy_ops_t *phy = NULL;

void i3c_sdr_init(void)
{
  bitbang_i3c_config.scl_pin = SCL_PIN;
  bitbang_i3c_config.sda_pin = SDA_PIN;

  bitbang_i3c_init(&bitbang_i3c_config);

  phy = &bitbang_i3c_ops;
}

bool i3c_sdr_transfer(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
  phy->start();

  /* 如果有发送数据，先走 Write Address */
  if (tx_len > 0) {

    if (!phy->write_byte((addr << 1) | 0)) {
      phy->stop();
      return false;
    }

    for (size_t i = 0; i < tx_len; i++) {
      if (!phy->write_byte(tx[i])) {
        phy->stop();
        return false;
      }
    }
  }

  /* 如果需要读取 */
  if (rx_len > 0) {

    /* 如果前面已经写过，则需要 Repeated START */
    if (tx_len > 0)
      phy->restart();
    else
      phy->start();

    if (!phy->write_byte((addr << 1) | 1)) {
      phy->stop();
      return false;
    }

    for (size_t i = 0; i < rx_len; i++) {

      bool ack = (i != rx_len - 1);

      rx[i] = phy->read_byte(ack);
    }
  }

  phy->stop();
  return true;
}

bool i3c_reg_read8(uint8_t dev, uint8_t reg, uint8_t *data)
{
  return i3c_sdr_transfer(dev, &reg, 1, data, 1);
}

bool i3c_reg_write8(uint8_t dev, uint8_t reg, uint8_t *data)
{
  uint8_t tx_buf[4];
  tx_buf[0] = reg;
  tx_buf[1] = *data;
  return i3c_sdr_transfer(dev, tx_buf, 2, NULL, 0);
}

bool i3c_reg_read16(uint8_t dev, uint8_t reg, uint16_t *data)
{
  return i3c_sdr_transfer(dev, &reg, 1, (uint8_t *) data, 2);
}

bool i3c_reg_write16(uint8_t dev, uint8_t reg, uint16_t *data)
{
  uint8_t tx_buf[4];
  tx_buf[0] = reg;
  tx_buf[1] = *data & 0xFF;
  tx_buf[2] = *data >> 8;
  return i3c_sdr_transfer(dev, tx_buf, 3, NULL, 0);
}

bool i3c_reg_read32(uint8_t dev, uint8_t reg, uint32_t *data)
{
  return i3c_sdr_transfer(dev, &reg, 1, (uint8_t *) data, 4);
}
bool i3c_reg_write32(uint8_t dev, uint8_t reg, uint32_t *data)
{
  uint8_t tx_buf[6];
  tx_buf[0] = reg;
  tx_buf[1] = *data & 0xFF;
  tx_buf[2] = (*data >> 8) & 0xFF;
  tx_buf[3] = (*data >> 16) & 0xFF;
  tx_buf[4] = (*data >> 24) & 0xFF;
  return i3c_sdr_transfer(dev, tx_buf, 5, NULL, 0);
}

bool i3c_reg_read(uint8_t dev, uint8_t reg, uint8_t *data, uint8_t len)
{
  return i3c_sdr_transfer(dev, &reg, 1, (uint8_t *) data, len);
}

bool i3c_reg_write(uint8_t dev, uint8_t reg, uint8_t *data, uint8_t len)
{
  uint8_t tx_buf[65];

  tx_buf[0] = reg;
  for (int i = 0; (i < len && i < 64); i++) {
    tx_buf[i + 1] = data[i];
  }
  return i3c_sdr_transfer(dev, tx_buf, len + 1, NULL, 0);
}