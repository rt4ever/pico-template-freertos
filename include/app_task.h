#ifndef APP_TASK_H
#define APP_TASK_H

#include "FreeRTOS.h"
#include "task.h"

#define APP_TASK_PRIORITY       (tskIDLE_PRIORITY + 1)
#define APP_TASK_STACK_SIZE     1024
#define APP_TASK_CORE_AFFINITY  (1 << 1)

void app_task(void *pvParameters);

#endif /* APP_TASK_H */
