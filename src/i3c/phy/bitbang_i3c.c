
#include "bitbang_i3c.h"
#include <hardware/gpio.h>
#include <stdbool.h>
#include <stdint.h>

#include <hardware/timer.h>

bitbang_i3c_config_t bitbang_i3c_config = {
    .scl_pin = SCL_PIN,
    .sda_pin = SDA_PIN,
};

const i3c_phy_ops_t bitbang_i3c_ops = {
    .start = bitbang_start,
    .stop = bitbang_stop,
    .restart = bitbang_restart,
    .read_byte = bitbang_read_byte,
    .write_byte = bitbang_write_byte,
};

static inline void sda_drive_low(void)
{
  gpio_set_dir(bitbang_i3c_config.sda_pin, GPIO_OUT);
  gpio_put(bitbang_i3c_config.sda_pin, 0);
}

static inline void sda_release(void) { gpio_set_dir(bitbang_i3c_config.sda_pin, GPIO_IN); }

static inline bool sda_read(void) { return gpio_get(bitbang_i3c_config.sda_pin); }

static inline void scl_high(void) { gpio_put(bitbang_i3c_config.scl_pin, 1); }

static inline void scl_low(void) { gpio_put(bitbang_i3c_config.scl_pin, 0); }

static inline void delay_half_cycle(void) { busy_wait_us(1); }

static inline void bus_idle(void)
{
  sda_release();
  scl_high();
  delay_half_cycle();
}

static inline void sda_drive(bool level)
{
  gpio_set_dir(bitbang_i3c_config.sda_pin, GPIO_OUT);
  gpio_put(bitbang_i3c_config.sda_pin, level);
}

static void write_bit(bool bit, bool open_drain)
{
  if (open_drain) {
    /*
         * Open Drain:
         *   0 -> Drive Low
         *   1 -> Release
         */
    if (bit)
      sda_release();
    else
      sda_drive_low();
  } else {
    /*
         * Push-Pull:
         *   0 -> Drive Low
         *   1 -> Drive High
         */
    sda_drive(bit);
  }

  delay_half_cycle();

  scl_high();
  delay_half_cycle();

  scl_low();
  //delay_half_cycle();
}

static bool read_bit(void)
{
  bool bit;

  /* Controller releases SDA */
  sda_release();

  delay_half_cycle();

  scl_high();
  delay_half_cycle();

  bit = sda_read();

  scl_low();
  //delay_half_cycle();

  return bit;
}

void bitbang_i3c_init(bitbang_i3c_config_t *cfg)
{
  gpio_init(cfg->scl_pin);
  gpio_init(cfg->sda_pin);

  gpio_set_dir(cfg->scl_pin, GPIO_OUT);
  //gpio_set_dir(cfg->sda_pin, GPIO_OUT);

  gpio_put(cfg->scl_pin, 1);
  //gpio_put(cfg->sda_pin, 1);
}

uint8_t bitbang_read_byte(bool ack)
{
  uint8_t data = 0;

  for (int i = 7; i >= 0; i--) {

    data <<= 1;

    if (read_bit())
      data |= 1;
  }

  /*
     * 第9位：
     * ack == true
     *      Controller ACK
     *
     * ack == false
     *      Controller NACK
     */

  write_bit(!ack, true);

  return data;
}

bool bitbang_write_byte(uint8_t data)
{
  for (int i = 7; i >= 0; i--) {
    write_bit((data >> i) & 0x01, false); // Push-Pull 发送数据
  }

  /* 第9位：读取 ACK */
  return !read_bit();
}

void bitbang_start(void)
{
  sda_release();
  scl_high();
  delay_half_cycle();

  sda_drive_low();
  delay_half_cycle();

  scl_low();
  delay_half_cycle();
}

void bitbang_restart(void)
{
  sda_release();
  delay_half_cycle();

  scl_high();
  delay_half_cycle();

  sda_drive_low();
  delay_half_cycle();

  scl_low();
  delay_half_cycle();
}

void bitbang_stop(void)
{
  sda_drive_low();
  delay_half_cycle();

  scl_high();
  delay_half_cycle();

  sda_release();
  delay_half_cycle();
}
