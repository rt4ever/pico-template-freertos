#include "tusb_task.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "pico/bootrom.h"
#include "pico/unique_id.h"

#include "tusb.h"

#include "app_task.h"
#include "build_info.h"

//--------------------------------------------------------------------+
// Constants
//--------------------------------------------------------------------+
#define BOOTSEL_MAGIC   "+++BOOTSEL+++"
#define INFO_MAGIC      "+++INFO+++"
#define CDC_RX_BUF_SIZE 64

//--------------------------------------------------------------------+
// Forward declarations
//--------------------------------------------------------------------+
static void print_system_info(void);

//--------------------------------------------------------------------+
// TinyUSB device task — pinned to core 0 (shares with FreeRTOS tick)
//--------------------------------------------------------------------+
void tusb_device_task(void *pvParameters) {
  (void)pvParameters;

  uint8_t rx_buf[CDC_RX_BUF_SIZE];
  static bool bootsel_triggered = false;

  while (1) {
    tud_task();

    if (tud_cdc_available()) {
      uint32_t count = tud_cdc_read(rx_buf, sizeof(rx_buf));

      if (count > 0) {
        char *rx_str = (char *)rx_buf;
        rx_str[(count < sizeof(rx_buf)) ? count : (sizeof(rx_buf) - 1)] = '\0';

        // --- Magic commands (do not echo the command itself) ---

        // Check for BOOTSEL magic string
        if (!bootsel_triggered && strstr(rx_str, BOOTSEL_MAGIC) != NULL) {
          bootsel_triggered = true;
          tud_cdc_write_str("\r\nBOOTSEL triggered — entering USB boot mode...\r\n");
          tud_cdc_write_flush();
          vTaskDelay(pdMS_TO_TICKS(10));
          reset_usb_boot(0, 0);
        }

        // Check for INFO magic string
        if (strstr(rx_str, INFO_MAGIC) != NULL) {
          print_system_info();
        } else {
          // Echo received data back
          tud_cdc_write(rx_buf, count);
          tud_cdc_write_flush();
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

//--------------------------------------------------------------------+
// Print system configuration over CDC
//--------------------------------------------------------------------+
static void print_system_info(void) {
  char buf[128];
  pico_unique_board_id_t board_id;
  pico_get_unique_board_id(&board_id);

  tud_cdc_write_str("\r\n");
  tud_cdc_write_str("============================================\r\n");
  tud_cdc_write_str("  RP2040 FreeRTOS Firmware — System Info\r\n");
  tud_cdc_write_str("============================================\r\n");

  // Build info
  snprintf(buf, sizeof(buf), "  Build:    %s %s\r\n", __DATE__, __TIME__);
  tud_cdc_write_str(buf);

  // Git info
  snprintf(buf, sizeof(buf), "  Git tag:  %s\r\n", BUILD_GIT_TAG);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Git hash: %s%s\r\n", BUILD_GIT_HASH_SHORT, BUILD_GIT_DIRTY_SUFFIX);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Branch:   %s\r\n", BUILD_GIT_BRANCH);
  tud_cdc_write_str(buf);

  // Board ID
  tud_cdc_write_str("  Board UID: ");
  for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
    snprintf(buf, sizeof(buf), "%02x", board_id.id[i]);
    tud_cdc_write_str(buf);
  }
  tud_cdc_write_str("\r\n");

  // FreeRTOS config
  snprintf(buf, sizeof(buf), "  RTOS cores: %u\r\n", (unsigned)configNUMBER_OF_CORES);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Tick rate:  %u Hz\r\n", (unsigned)configTICK_RATE_HZ);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Max prio:   %u\r\n", (unsigned)configMAX_PRIORITIES);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Heap size:  %u bytes (Heap1)\r\n", (unsigned)configTOTAL_HEAP_SIZE);
  tud_cdc_write_str(buf);

  // Core affinity
  tud_cdc_write_str(
      "  Affinity:   core0 = TinyUSB + tick | core1 = application\r\n");

  // USB config
  tud_cdc_write_str("  USB:        CDC-ACM (VID 0x2E8A PID 0x000C)\r\n");

  // TinyUSB task
  snprintf(buf, sizeof(buf),
      "  tusb task:  prio %u, stack %u, core %u\r\n",
      (unsigned)TUSB_TASK_PRIORITY, (unsigned)TUSB_TASK_STACK_SIZE,
      (unsigned)(TUSB_TASK_CORE_AFFINITY & 1 ? 0 : 1));
  tud_cdc_write_str(buf);

  // App task
  snprintf(buf, sizeof(buf),
      "  app task:   prio %u, stack %u, core %u\r\n",
      (unsigned)APP_TASK_PRIORITY, (unsigned)APP_TASK_STACK_SIZE,
      (unsigned)(APP_TASK_CORE_AFFINITY & 1 ? 0 : 1));
  tud_cdc_write_str(buf);

  tud_cdc_write_str("============================================\r\n");
  tud_cdc_write_flush();
}
