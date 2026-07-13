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
#include "pio_hw.h"

#include "i3c_sdr.h"

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
static void cmd_toggle(int argc, char **argv);
static void cmd_i3c(int argc, char **argv);

static const cmd_t cmd_table[] = {
    {"help", cmd_help, "Show available commands"},
    {"info", cmd_info, "System information"},
    {"bootsel", cmd_bootsel, "Enter USB boot mode"},
    {"pio", cmd_toggle, "PIO toggle: on|off (GPIO27)"},
    {"i3c", cmd_i3c, "I3C Xfer: (SCL-GPIO16, SDA-GPIO17)"},
    {NULL, NULL, NULL},
};

//--------------------------------------------------------------------+
// Line buffer
//--------------------------------------------------------------------+
#define LINE_BUF_SIZE 128
static char line_buf[LINE_BUF_SIZE];
static int line_pos = 0;


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
// PIO Toggle command
//--------------------------------------------------------------------+
static void cmd_toggle(int argc, char **argv)
{
  if (argc < 2) {
    tud_cdc_write_str("\r\nUsage: pio <on|off>\r\n> ");
    tud_cdc_write_flush();
    return;
  }

  bool on;
  if (strcmp(argv[1], "on") == 0) {
    on = true;
  } else if (strcmp(argv[1], "off") == 0) {
    on = false;
  } else {
    tud_cdc_write_str("\r\nUsage: pio <on|off>\r\n> ");
    tud_cdc_write_flush();
    return;
  }

  bool running = pio_toggle_switch(on);
  tud_cdc_write_str(running ? "\r\nPIO toggle ON  (GPIO27)\r\n> "
                            : "\r\nPIO toggle OFF (GPIO27)\r\n> ");
  tud_cdc_write_flush();
}

//--------------------------------------------------------------------+
// PIO I3C Xfer command
//--------------------------------------------------------------------+
char cdc_buf[64];
char data_buf[64];
static void cmd_i3c(int argc, char **argv)
{
  if (argc < 3) {
    tud_cdc_write_str("\r\nUsage: i3c <read|write> <dev:reg[:data]>\r\n"
                      "  read  08:00        — read reg 0x00 from device 0x08\r\n"
                      "  write 08:00:AA     — write 0xAA to device 0x08 reg 0x00\r\n"
                      "  (all values in hex, 0x prefix optional)\r\n"
                      "> ");
    tud_cdc_write_flush();
    return;
  }

  bool is_read;
  if (strcmp(argv[1], "read") == 0) {
    is_read = true;
  } else if (strcmp(argv[1], "write") == 0) {
    is_read = false;
  } else {
    tud_cdc_write_str("\r\n? Unknown operation. Use 'read' or 'write'.\r\n> ");
    tud_cdc_write_flush();
    return;
  }

  // Parse DEV:REG[:DATA] from argv[2]
  unsigned dev = 0, reg = 0, data = 0, len = 0;
  int n;
  if (is_read) {
    n = sscanf(argv[2], "%x:%x:%x", &dev, &reg, &len);
  } else {
    n = sscanf(argv[2], "%x:%x:%x", &dev, &reg, &data);
  }

  if (n < 2) {
    tud_cdc_write_str("\r\n? Bad address format. Use dev:reg[:data] (hex).\r\n> ");
    tud_cdc_write_flush();
    return;
  }

  if (is_read) {
    if (n == 2) {
      i3c_reg_read8(dev, reg, (uint8_t *) &data);
      snprintf(cdc_buf, sizeof(cdc_buf),
               "\r\nI3C read: dev=0x%02X reg=0x%02X → data=0x%02X (stub)\r\n> ", dev, reg, data);
      tud_cdc_write_str(cdc_buf);
      tud_cdc_write_flush();
      return;

    } else if (n == 3) {
      i3c_reg_read(dev, reg, (uint8_t *) data_buf, len);
      snprintf(cdc_buf, sizeof(cdc_buf), "\r\nI3C read: dev=0x%02X reg=0x%02X → data=:\r\n> ", dev,
               reg);
      tud_cdc_write_str(cdc_buf);
      for (int i = 0; i < len; i++) {
        snprintf(cdc_buf, sizeof(cdc_buf), "\t0x%02X\r\n", data_buf[i]);
        tud_cdc_write_str(cdc_buf);
      }
      tud_cdc_write_flush();
      return;

    } else {
      tud_cdc_write_str("\r\n? Write requires dev:reg (2 hex values).\r\n> ");
    }

  } else {

    if (n == 3) {
      i3c_reg_write8(dev, reg, (uint8_t *) &data);
      tud_cdc_write_str("\r\n");
      tud_cdc_write_flush();
    } else {
      tud_cdc_write_str("\r\n? Write requires dev:reg:data (3 hex values).\r\n> ");
    }
  }
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
