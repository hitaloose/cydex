#include "power.h"

#include <lvgl.h>

#include "data/Settings.h"
#include "hal/backlight.h"
#include "hal/speaker.h"
#include "hal/touch.h"
#include "ui/nav.h"
#include "util/log.h"

static uint8_t s_level = 70;
static bool s_asleep = false;
static bool s_swallow = false;  // engole o toque que acordou até o dedo sair

static void wake() {
  s_asleep = false;
  backlight_set(s_level);
  lv_disp_trig_activity(nullptr);
  LOG_I("power", "tela acesa");
}

static bool touch_filter(bool pressed) {
  if (s_asleep && pressed) {
    wake();
    s_swallow = true;
  }
  if (s_swallow) {
    if (!pressed) s_swallow = false;
    return false;
  }
  return true;
}

static void on_press() {
  if (settings().sound) speaker_click();
}

void power_init() {
  speaker_init();
  touch_set_filter(touch_filter);
  touch_set_press_hook(on_press);
  power_use_normal_level();
}

void power_set_level(uint8_t percent) {
  s_level = percent;
  if (!s_asleep) backlight_set(percent);
}

void power_use_normal_level() { power_set_level(settings().brightness); }

void power_tick() {
  speaker_update();
  uint16_t minutes = settings().screen_off_min;
  if (s_asleep || minutes == 0 || nav_current() == Screen::Showcase) return;
  if (lv_disp_get_inactive_time(nullptr) >= (uint32_t)minutes * 60000u) {
    s_asleep = true;
    backlight_set(0);
    LOG_I("power", "tela apagada (%u min sem toque)", minutes);
  }
}
