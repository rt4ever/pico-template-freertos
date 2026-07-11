#include "pio_hw.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "pio_toggle.pio.h" // pioasm-generated header

//--------------------------------------------------------------------+
// PIO instance + state-machine handle
//--------------------------------------------------------------------+
static PIO pio_toggle_pio;
static uint pio_toggle_sm;
static bool pio_toggle_running = false;

//--------------------------------------------------------------------+
// Initialise PIO toggle state machine on GPIO27
//--------------------------------------------------------------------+
void pio_inst_init(void)
{

  pio_toggle_pio = pio0;
  pio_toggle_sm = pio_claim_unused_sm(pio_toggle_pio, true);

  uint offset = pio_add_program(pio_toggle_pio, &pio_toggle_program);

  pio_toggle_program_init(pio_toggle_pio, pio_toggle_sm, offset, PIO_TOGGLE_PIN_BASE);

  // Start running by default
  pio_sm_set_enabled(pio_toggle_pio, pio_toggle_sm, true);
  pio_toggle_running = true;
}

//--------------------------------------------------------------------+
// Enable / disable toggle — returns new state
//--------------------------------------------------------------------+
bool pio_toggle_switch(bool on)
{
  pio_toggle_running = on;
  pio_sm_set_enabled(pio_toggle_pio, pio_toggle_sm, on);
  if (!on) {
    gpio_put(PIO_TOGGLE_PIN_BASE, 0);
  }
  return pio_toggle_running;
}
