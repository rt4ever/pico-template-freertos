#include "app_disp_task.h"
#include "platform_hw.h"

#include "lvgl.h"
#include "lvgl/draw/lv_palette.h"

#include <lvgl/core/lv_obj_style.h>
#include <lvgl/draw/lv_color.h>
#include <stdio.h>

//--------------------------------------------------------------------+
// LVGL display buffer: 240 px × 10 rows × 2 B/px (RGB565) = 4800 B
//--------------------------------------------------------------------+
static uint8_t disp_buf[DISP_HOR_RES * 10 * 2];

//--------------------------------------------------------------------+
// Timer state
//--------------------------------------------------------------------+
uint8_t t_h = 0, t_m = 0, t_s = 0; // extern: tusb_task reads for 'time' command
static uint8_t page = 0;
static lv_obj_t *time_label;
static lv_obj_t *scr;

//--------------------------------------------------------------------+
// Flush callback
//--------------------------------------------------------------------+
static void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  st7789_set_window(area->x1, area->y1, area->x2, area->y2);
  st7789_write_pixels(px_map, w * h * 2);
  lv_display_flush_ready(disp);
}

//--------------------------------------------------------------------+
// Flip reset — restore background 300 ms after a minute rollover
//--------------------------------------------------------------------+
static void flip_reset_cb(lv_timer_t *t)
{
  (void) t;
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
}

//--------------------------------------------------------------------+
// Every-second tick — update HH:MM:SS, trigger flip on minute rollover
//--------------------------------------------------------------------+
static void timer_tick_cb(lv_timer_t *t)
{
  (void) t;

  t_s++;
  bool minute_roll = false;

  if (t_s >= 60) {
    t_s = 0;
    t_m++;
    minute_roll = true;
  }
  if (t_m >= 60) {
    t_m = 0;
    t_h++;
  }
  if (t_h >= 100) {
    t_h = 0;
  }

  // Update label
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", t_h, t_m, t_s);
  lv_label_set_text(time_label, buf);

  // Flip effect on minute rollover
  if (minute_roll) {
    page++;
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003366), 0); // blue flash
    lv_timer_create(flip_reset_cb, 300, NULL);
  }
}

//--------------------------------------------------------------------+
// Display task — pinned to core 1
//--------------------------------------------------------------------+
void app_disp_task(void *pvParameters)
{
  (void) pvParameters;

  st7789_init();

  lv_init();
  lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
  lv_display_set_flush_cb(disp, my_flush_cb);
  lv_display_set_buffers(disp, disp_buf, NULL, sizeof(disp_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_make(0x12, 0x12, 0x12), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_80, 0);

  // Timer label — centred, large white text
  time_label = lv_label_create(scr);
  lv_label_set_text(time_label, "00:00:00");
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_18, 0);

  lv_obj_set_style_text_color(time_label, lv_color_make(0xE0, 0xE0, 0xE0), LV_STATE_FOCUS_KEY);
  lv_obj_center(time_label);

  // 1-second LVGL software timer
  lv_timer_create(timer_tick_cb, 1000, NULL);

  for (;;) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
