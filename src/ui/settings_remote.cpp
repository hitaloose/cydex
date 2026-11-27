#include "settings_remote.h"

#include "data/Settings.h"
#include "i18n/i18n.h"
#include "logic/SettingsPatch.h"
#include "ui/nav.h"
#include "ui/power.h"
#include "util/log.h"

// Telas que mostram os Ajustes: remontam com os valores novos.
static bool shows_settings(Screen s) {
  switch (s) {
    case Screen::Settings:
    case Screen::SettingsDisplay:
    case Screen::SettingsNavigation:
    case Screen::SettingsShowcase:
    case Screen::SettingsSystem:
      return true;
    default:
      return false;
  }
}

// Remontar perderia o que a pessoa está digitando ou escolhendo.
static bool busy_screen(Screen s) {
  return s == Screen::Search || s == Screen::WifiPassword || s == Screen::SettingsChoice;
}

void settings_remote_apply(const SettingsPatch &p) {
  AppSettings &st = settings();
  if (p.has_bright) st.brightness = p.bright;
  if (p.has_bright_n) st.brightness_night = p.bright_n;
  if (p.has_off) st.screen_off_min = p.off_min;
  if (p.has_sound) st.sound = p.sound;
  if (p.has_grid) st.open_grid = p.grid;
  if (p.has_sc_int) st.showcase_interval_s = p.sc_int;
  if (p.has_sc_rand) st.showcase_random = p.sc_rand;
  if (p.has_sc_src) st.showcase_source = p.sc_src;
  if (p.has_sc_boot) st.showcase_on_boot = p.sc_boot;
  settings_save();
  if (p.has_lang) settings_set_lang(i18n_lang_from_code(p.lang));
  if (p.has_bright || p.has_bright_n) power_settings_changed();
  LOG_I("settings", "mudados pelo app");

  Screen cur = nav_current();
  bool showcase = p.has_sc_int || p.has_sc_rand || p.has_sc_src;
  if (shows_settings(cur) || (cur == Screen::Showcase && showcase) ||
      (p.has_lang && !busy_screen(cur))) {
    nav_reload();
  }
}
