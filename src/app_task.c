#include "app_task.h"

//--------------------------------------------------------------------+
// Application task — pinned to core 1
//--------------------------------------------------------------------+
void app_task(void *pvParameters) {
  (void)pvParameters;

  while (1) {
    // Placeholder for application logic
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
