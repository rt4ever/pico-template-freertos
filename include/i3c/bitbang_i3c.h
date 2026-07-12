#ifndef BITBANG_I3C_H
#define BITBANG_I3C_H

#include <stdbool.h>
#include <stdint.h>

#define SDA_PIN 8
#define SCL_PIN 9

typedef struct {

  void (*start)(void);
  void (*restart)(void);
  void (*stop)(void);

  bool (*write_byte)(uint8_t data);
  uint8_t (*read_byte)(bool ack);

} i3c_phy_ops_t;

typedef struct {

  uint8_t scl_pin;
  uint8_t sda_pin;

} bitbang_i3c_config_t;

void bitbang_i3c_init(bitbang_i3c_config_t *cfg);

void bitbang_start(void);
void bitbang_restart(void);
void bitbang_stop(void);
uint8_t bitbang_read_byte(bool ack);
bool bitbang_write_byte(uint8_t data);

extern const i3c_phy_ops_t bitbang_i3c_ops;
extern bitbang_i3c_config_t bitbang_i3c_config;

#endif