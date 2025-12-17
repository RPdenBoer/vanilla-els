#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "coordinates.h"
#include "display.h"
#include "encoder.h"
#include "modal.h"
#include "offsets.h"
#include "tools.h"
#include "touch.h"
#include "ui.h"
#include "leadscrew.h"

/* =========================
   LVGL timing
   ========================= */
static uint32_t last_lv_ms = 0;

static void ui_timer_cb(lv_timer_t *t)
{
  (void)t;
  EncoderManager::update();
  UIManager::update();
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.printf("boot: %s %s\n", __DATE__, __TIME__);
  Serial.flush();

  if (!DisplayManager::init()) {
    Serial.println("panel begin failed");
    while (true) delay(1000);
  }
  Serial.println("display ok");

  bool t_ok = TouchManager::init();
  Serial.printf("touch init: %s\n", t_ok ? "OK" : "FAIL");

  bool enc_ok = EncoderManager::init();
  Serial.printf("pcnt init: %s (A=%d B=%d)\n", enc_ok ? "OK" : "FAIL", C_PINA, C_PINB);

  lv_init();
  Serial.println("lvgl init");

  // Partial buffers (40 lines)
  static lv_color_t buf1[SCREEN_W * 40];
  static lv_color_t buf2[SCREEN_W * 40];

  lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, DisplayManager::flushCallback);
  lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, TouchManager::readCallback);

  // Dark theme
  lv_theme_t *th = lv_theme_default_init(
      lv_display_get_default(),
      lv_palette_main(LV_PALETTE_BLUE),
      lv_palette_main(LV_PALETTE_RED),
      true,
      LV_FONT_DEFAULT);
  lv_display_set_theme(lv_display_get_default(), th);

  ToolManager::init();
  OffsetManager::init();
  UIManager::init();
  Serial.println("ui init");

  LeadscrewManager::init();
  Serial.println("els init");

  EncoderManager::update();
  UIManager::update();

  lv_timer_create(ui_timer_cb, 50, nullptr);
  last_lv_ms = millis();
  Serial.println("boot complete");
}

void loop()
{
  uint32_t now = millis();
  uint32_t diff = now - last_lv_ms;
  last_lv_ms = now;

  lv_tick_inc(diff);
  lv_timer_handler();

  static uint32_t last_hb = 0;
  if (now - last_hb > 1000) {
    last_hb = now;
    Serial.println("hb");
  }

  delay(2);
}
