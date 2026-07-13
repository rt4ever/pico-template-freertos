#include "tusb_task.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "class/cdc/cdc_device.h"
#include "task.h"

#include "pico/bootrom.h"
#include "pico/unique_id.h"

#include "tusb.h"

#include "app_disp_task.h"
#include "build_info.h"


//--------------------------------------------------------------------+
// Command handler type + table entry
//--------------------------------------------------------------------+
typedef void (*cmd_fn)(int argc, char **argv);

typedef struct {
  const char *name;
  cmd_fn handler;
  const char *help;
} cmd_t;

//--------------------------------------------------------------------+
// Forward declarations
//--------------------------------------------------------------------+
static void cmd_help(int argc, char **argv);
static void cmd_info(int argc, char **argv);
static void cmd_bootsel(int argc, char **argv);
static void cmd_time(int argc, char **argv);


static const cmd_t cmd_table[] = {
    {"help", cmd_help, "Show available commands"},
    {"info", cmd_info, "System information"},
    {"bootsel", cmd_bootsel, "Enter USB boot mode"},
    {"time", cmd_time, "Show timer value"},
    {NULL, NULL, NULL},
};

//--------------------------------------------------------------------+
// Line buffer
//--------------------------------------------------------------------+
#define LINE_BUF_SIZE 128
static char line_buf[LINE_BUF_SIZE];
static int line_pos = 0;

//--------------------------------------------------------------------+
// Extern: timer state from app_disp_task.c
//--------------------------------------------------------------------+
extern uint8_t t_h, t_m, t_s;

//--------------------------------------------------------------------+
// Help command
//--------------------------------------------------------------------+
static void cmd_help(int argc, char **argv)
{
  (void) argc;
  (void) argv;
  tud_cdc_write_str("\r\nCommands:\r\n");
  for (const cmd_t *c = cmd_table; c->name; c++) {
    tud_cdc_write_str(" ");
    tud_cdc_write_str(c->name);
    tud_cdc_write_str(" — ");
    tud_cdc_write_str(c->help);
    tud_cdc_write_str("\r\n");
  }
  tud_cdc_write_str("\r\n> ");
  tud_cdc_write_flush();
}

//--------------------------------------------------------------------+
// Info command
//--------------------------------------------------------------------+
static void cmd_info(int argc, char **argv)
{
  (void) argc;
  (void) argv;

  char buf[128];
  pico_unique_board_id_t board_id;
  pico_get_unique_board_id(&board_id);

  tud_cdc_write_str("\r\n");
  tud_cdc_write_str("============================================\r\n");
  tud_cdc_write_str("  RP2040 FreeRTOS Firmware — System Info\r\n");
  tud_cdc_write_str("============================================\r\n");

  snprintf(buf, sizeof(buf), "  Build:    %s %s\r\n", __DATE__, __TIME__);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Git tag:  %s\r\n", BUILD_GIT_TAG);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Git hash: %s%s\r\n", BUILD_GIT_HASH_SHORT, BUILD_GIT_DIRTY_SUFFIX);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Branch:   %s\r\n", BUILD_GIT_BRANCH);
  tud_cdc_write_str(buf);

  tud_cdc_write_str("  Board UID: ");
  for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
    snprintf(buf, sizeof(buf), "%02x", board_id.id[i]);
    tud_cdc_write_str(buf);
  }
  tud_cdc_write_str("\r\n");

  snprintf(buf, sizeof(buf), "  RTOS cores: %u\r\n", (unsigned) configNUMBER_OF_CORES);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Tick rate:  %u Hz\r\n", (unsigned) configTICK_RATE_HZ);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Max prio:   %u\r\n", (unsigned) configMAX_PRIORITIES);
  tud_cdc_write_str(buf);
  snprintf(buf, sizeof(buf), "  Heap size:  %u bytes (Heap1)\r\n",
           (unsigned) configTOTAL_HEAP_SIZE);
  tud_cdc_write_str(buf);

  tud_cdc_write_str("  Affinity:   core0 = TinyUSB + tick | core1 = display\r\n");
  tud_cdc_write_str("  USB:        CDC-ACM (VID 0x2E8A PID 0x000C)\r\n");

  snprintf(buf, sizeof(buf), "  tusb task:  prio %u, stack %u, core %u\r\n",
           (unsigned) TUSB_TASK_PRIORITY, (unsigned) TUSB_TASK_STACK_SIZE,
           (unsigned) (TUSB_TASK_CORE_AFFINITY & 1 ? 0 : 1));
  tud_cdc_write_str(buf);

  snprintf(buf, sizeof(buf), "  disp task:  prio %u, stack %u, core %u\r\n",
           (unsigned) APP_DISP_TASK_PRIORITY, (unsigned) APP_DISP_TASK_STACK_SIZE,
           (unsigned) (APP_DISP_TASK_CORE_AFFINITY & 1 ? 0 : 1));
  tud_cdc_write_str(buf);

  snprintf(buf, sizeof(buf), "  Timer:      %02u:%02u:%02u\r\n", t_h, t_m, t_s);
  tud_cdc_write_str(buf);

  tud_cdc_write_str("============================================\r\n");
  tud_cdc_write_str("> ");
  tud_cdc_write_flush();
}

//--------------------------------------------------------------------+
// Bootsel command
//--------------------------------------------------------------------+
static void cmd_bootsel(int argc, char **argv)
{
  (void) argc;
  (void) argv;
  tud_cdc_write_str("\r\nEntering USB boot mode...\r\n");
  tud_cdc_write_flush();
  vTaskDelay(pdMS_TO_TICKS(10));
  reset_usb_boot(0, 0);
}

//--------------------------------------------------------------------+
// Time command
//--------------------------------------------------------------------+
static void cmd_time(int argc, char **argv)
{
  (void) argc;
  (void) argv;
  char buf[32];
  snprintf(buf, sizeof(buf), "\r\nTimer: %02u:%02u:%02u\r\n> ", t_h, t_m, t_s);
  tud_cdc_write_str(buf);
  tud_cdc_write_flush();
}


//--------------------------------------------------------------------+
// Line parser — split into argc/argv, dispatch to command table
//--------------------------------------------------------------------+
static void process_line(char *line)
{
  // Tokenise
  char *argv[8];
  int argc = 0;
  char *tok = strtok(line, " \t\r\n");
  while (tok && argc < 8) {
    argv[argc++] = tok;
    tok = strtok(NULL, " \t\r\n");
  }
  if (argc == 0) {
    tud_cdc_write_str("> ");
    tud_cdc_write_flush();
    return;
  }

  // Dispatch
  for (const cmd_t *c = cmd_table; c->name; c++) {
    if (strcmp(argv[0], c->name) == 0) {
      c->handler(argc, argv);
      return;
    }
  }
  tud_cdc_write_str("\r\nUnknown command. Type 'help'.\r\n> ");
  tud_cdc_write_flush();
}

//--------------------------------------------------------------------+
// TinyUSB device task — pinned to core 0
//--------------------------------------------------------------------+
void tusb_device_task(void *pvParameters)
{
  (void) pvParameters;

  uint8_t rx_buf[64];

  tud_cdc_write_str("\r\n> ");
  tud_cdc_write_flush();

  while (1) {
    tud_task();

    if (tud_cdc_available()) {
      uint32_t count = tud_cdc_read(rx_buf, sizeof(rx_buf));

      for (uint32_t i = 0; i < count; i++) {
        char ch = (char) rx_buf[i];

        // Echo printable characters
        if (ch == '\b' || ch == 0x7F) {
          if (line_pos > 0) {
            line_pos--;
            tud_cdc_write_str("\b \b"); // backspace
          }
        } else if (ch == '\r' || ch == '\n') {
          if (line_pos > 0) {
            tud_cdc_write_str("\r\n");
            line_buf[line_pos] = '\0';
            process_line(line_buf);
            line_pos = 0;
          }
        } else if (ch >= 0x20 && ch < 0x7F && line_pos < LINE_BUF_SIZE - 1) {
          line_buf[line_pos++] = ch;
          tud_cdc_write(&ch, 1); // echo
        }
        // ignore other control chars
      }
      tud_cdc_write_flush();
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
