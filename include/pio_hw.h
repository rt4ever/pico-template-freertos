#ifndef PIO_HW_H
#define PIO_HW_H

#include "hardware/pio.h"

//--------------------------------------------------------------------+
// PIO configuration — GPIO27 TOGGLE ON PIO0
//--------------------------------------------------------------------+
#define PIO_TOGGLE_PIN_BASE 27
#define PIO_TOGGLE_PIN_MASK (1 << 27)

//--------------------------------------------------------------------+
// PIO configuration — I3C (GPIO16 & GPIO17) PIO1
//--------------------------------------------------------------------+
#define PIO_I3C_PIN_BASE 16
#define PIO_I3C_PIN_MASK ((1 << 16) | (1 << 17))

//--------------------------------------------------------------------+
// API
//--------------------------------------------------------------------+
void pio_inst_init(void);
bool pio_toggle_switch(bool on); //if PIO Toggle SM is on, then swith off


#endif /* PIO_HW_H */
