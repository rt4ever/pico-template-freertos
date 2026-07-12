#include "FreeRTOS.h"
#include "task.h"

#include <pico/stdlib.h>
#include <pico/binary_info.h>
#include <stdio.h>

#include "bsp/board_api.h"

#include "tusb.h"

#include "tusb_task.h"
#include "app_disp_task.h"
#include "platform_hw.h"
#include "pio_hw.h"
#include "lvgl.h"
#include "build_info.h"

#include "i3c_sdr.h"

//--------------------------------------------------------------------+
// Binary info declarations (readable via picotool info)
//--------------------------------------------------------------------+
bi_decl(bi_program_name("pico-template-freertos"));
bi_decl(bi_program_description("Pico FreeRTOS SMP firmware with CDC-ACM USB echo"));
bi_decl(bi_program_version_string(BUILD_GIT_TAG));
bi_decl(bi_program_build_date_string(__DATE__ " " __TIME__));
bi_decl(bi_program_url("https://github.com/rt4ever/pico-template-freertos"));
bi_decl(bi_program_feature("FreeRTOS SMP (2 cores)"));
bi_decl(bi_program_feature("TinyUSB CDC-ACM echo"));
bi_decl(bi_program_feature("Magic command: +++BOOTSEL+++ / +++INFO+++"));
bi_decl(bi_program_build_attribute("Toolchain: arm-none-eabi-gcc"));
bi_decl(bi_program_build_attribute("Build type: RelWithDebInfo"));
bi_decl(bi_program_build_attribute("Git: " BUILD_GIT_BRANCH
                                   " " BUILD_GIT_HASH_SHORT BUILD_GIT_DIRTY_SUFFIX));

//--------------------------------------------------------------------+
// main
//--------------------------------------------------------------------+
int main(void)
{

  //pico-sdk init
  board_init();

  //from specifi hardware
  platform_init();

  //pio
  pio_inst_init();

  //i3c
  i3c_sdr_init();

  tusb_init();
  stdio_uart_init();

  // Create TinyUSB task and pin to core 0
  TaskHandle_t tusb_task_handle = NULL;
  BaseType_t ret = xTaskCreate(tusb_device_task, "tusb", TUSB_TASK_STACK_SIZE, NULL,
                               TUSB_TASK_PRIORITY, &tusb_task_handle);
  if (ret == pdPASS) {
    vTaskCoreAffinitySet(tusb_task_handle, TUSB_TASK_CORE_AFFINITY);
  }

  // Create display task and pin to core 1
  TaskHandle_t disp_task_handle = NULL;
  ret = xTaskCreate(app_disp_task, "disp", APP_DISP_TASK_STACK_SIZE, NULL, APP_DISP_TASK_PRIORITY,
                    &disp_task_handle);
  if (ret == pdPASS) {
    vTaskCoreAffinitySet(disp_task_handle, APP_DISP_TASK_CORE_AFFINITY);
  }

  // Start scheduler — never returns on success
  vTaskStartScheduler();

  // Should never reach here
  panic("vTaskStartScheduler returned\n");
  return 0;
}

//--------------------------------------------------------------------+
// FreeRTOS hooks
//--------------------------------------------------------------------+
void vApplicationTickHook(void)
{
  lv_tick_inc(1); // LVGL heartbeat @ 1 kHz
};

void vApplicationStackOverflowHook(TaskHandle_t Task, char *pcTaskName)
{
  panic("stack overflow (not the helpful kind) for %s\n", *pcTaskName);
}

void vApplicationMallocFailedHook(void) { panic("Malloc Failed\n"); };
