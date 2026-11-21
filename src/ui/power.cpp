#include "power.h"

#include <Arduino.h>
#include <lvgl.h>

#include <ctime>

#include "data/Settings.h"
#include "hal/backlight.h"
#include "hal/speaker.h"
#include "hal/touch.h"
#include "logic/Sun.h"
#include "net/notice_server.h"
#include "ui/nav.h"
#include "util/log.h"

static uint8_t s_level = 70;
// Qual brilho dos Ajustes vale agora. Fixed = power_set_level (prévia).
enum class Mode : uint8_t { Fixed, Normal, Showcase };
static Mode s_mode = Mode::Normal;
static bool s_night = false;
static uint32_t s_sun_check = 0;  // millis() da última conta do sol
static bool s_asleep = false;
// O computador dormiu (POST /sleep). Enquanto houver aviso aberto a tela fica
// acesa; quando o último fecha, apaga de novo. Toque ou /wake desfazem.
static bool s_remote = false;
static bool s_swallow = false;  // engole o toque que acordou até o dedo sair

static void wake() {
  s_asleep = false;
  backlight_set(s_level);
  lv_disp_trig_activity(nullptr);
  LOG_I("power", "tela acesa");
}

static bool touch_filter(bool pressed) {
  if (s_asleep && pressed) {
    s_remote = false;  // quem tocou quer usar o CYD
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

static void apply_level(uint8_t percent) {
  s_level = percent;
  if (!s_asleep) backlight_set(percent);
}

static bool compute_night() {
  const AppSettings &st = settings();
  if (!st.has_location) return false;
  time_t now = time(nullptr);
  if ((int64_t)now < SUN_VALID_TIME) return false;  // sem NTP ainda
  return sun_is_night((int64_t)now, st.lat, st.lon);
}

static void apply_mode() {
  const AppSettings &st = settings();
  if (s_mode == Mode::Normal) apply_level(s_night ? st.brightness_night : st.brightness);
  else if (s_mode == Mode::Showcase)
    apply_level(s_night ? st.showcase_brightness_night : st.showcase_brightness);
}

// Recalcula o dia/noite; reaplica o brilho se mudou (ou se `force`).
static void sun_update(bool force) {
  s_sun_check = millis();
  bool night = compute_night();
  if (night == s_night && !force) return;
  if (night != s_night) LOG_I("power", "%s", night ? "anoiteceu" : "amanheceu");
  s_night = night;
  apply_mode();
}

void power_set_level(uint8_t percent) {
  s_mode = Mode::Fixed;
  apply_level(percent);
}

void power_use_normal_level() {
  s_mode = Mode::Normal;
  sun_update(true);
}

void power_use_showcase_level() {
  s_mode = Mode::Showcase;
  sun_update(true);
}

bool power_is_night() { return s_night; }

void power_location_changed() { sun_update(true); }

static void remote_off() {
  s_asleep = true;
  backlight_set(0);
  LOG_I("power", "tela apagada (computador dormindo, sem avisos)");
}

void power_tick() {
  speaker_update();
  // Uma vez por minuto basta (e pega o NTP chegando depois do boot).
  if (millis() - s_sun_check >= 60000u) sun_update(false);
  if (s_remote) {
    if (!s_asleep && notices().size() == 0) remote_off();
    return;
  }
  uint16_t minutes = settings().screen_off_min;
  if (s_asleep || minutes == 0 || nav_current() == Screen::Showcase) return;
  if (lv_disp_get_inactive_time(nullptr) >= (uint32_t)minutes * 60000u) {
    s_asleep = true;
    backlight_set(0);
    LOG_I("power", "tela apagada (%u min sem toque)", minutes);
  }
}

void power_remote_sleep() {
  s_remote = true;
  if (!s_asleep && notices().size() == 0) remote_off();
}

void power_remote_wake() {
  s_remote = false;
  power_wake();
}

void power_wake() {
  if (s_asleep) wake();
  else lv_disp_trig_activity(nullptr);
}
