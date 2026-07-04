#include "platform_hw.h"
#include "display_test.h"
#include "app_task.h"

//--------------------------------------------------------------------+
// Application task — pinned to core 1
//--------------------------------------------------------------------+
void app_task(void *pvParameters) {
  (void)pvParameters;

  st7789_init();
  display_test_run();  // checkerboard <-> colour card, 1 s toggle
}