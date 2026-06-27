#ifndef TUSB_TASK_H
#define TUSB_TASK_H

#include "FreeRTOS.h"
#include "task.h"

#define TUSB_TASK_PRIORITY      (configMAX_PRIORITIES - 2)
#define TUSB_TASK_STACK_SIZE    2048
#define TUSB_TASK_CORE_AFFINITY (1 << 0)

void tusb_device_task(void *pvParameters);

#endif /* TUSB_TASK_H */
