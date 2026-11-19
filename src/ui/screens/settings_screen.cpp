// 05 Ajustes (canvas: Ajustes.dc.html; regras na nota "Ajustes: regras").
//   linhas de 34 px: Brilho, Som ao tocar, Abrir em, Apagar tela,
//   Modo vitrine ›, Sistema › (a última ocupa o resto da altura)
#include <cstdio>

#include "data/Settings.h"
#include "i18n/i18n.h"
#include "logic/Showcase.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/icons/icons.h"
#include "ui/power.h"
#include "ui/settings_widgets.h"
#include "ui/theme.h"
#include "ui/widgets.h"

// "1 min" / "30 s": também usado no 05b.
void format_interval(uint16_t seconds, char *out, size_t size) {
  if (seconds < 60) snprintf(out, size, tr(STR_SET_SECONDS_FMT), seconds);
  else snprintf(out, size, tr(STR_SET_MINUTES_FMT), seconds / 60);
}

namespace {

constexpr lv_coord_t kRowH = 34;
const StrId kViews[2] = {STR_TITLE_LIST, STR_TITLE_GRID};

lv_obj_t *s_views;
Stepper s_off;

void on_back(lv_event_t *) { nav_go(Screen::Menu); }

void on_brightness(lv_event_t *e) {
  uint8_t v = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
  settings().brightness = v;
  power_set_level(v);
}

void on_save(lv_event_t *) { settings_save(); }

void on_sound(lv_event_t *e) {
  settings().sound = set_switch_get(lv_event_get_current_target(e));
  settings_save();
}

void on_view(lv_event_t *e) {
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  settings().open_grid = i == 1;
  catalog_state().view = i == 1 ? Screen::Grid : Screen::List;
  set_segmented_select(s_views, i);
  settings_save();
}

void refresh_off() {
  uint16_t m = settings().screen_off_min;
  char text[16];
  if (m == 0) snprintf(text, sizeof(text), "%s", tr(STR_SET_NEVER));
  else snprintf(text, sizeof(text), tr(STR_SET_MINUTES_FMT), m);
  uint8_t i = option_index(SCREEN_OFF_MIN, SCREEN_OFF_COUNT, m);
  set_stepper_update(s_off, text, i == 0, i == SCREEN_OFF_COUNT - 1);
}

void on_off_step(lv_event_t *e) {
  int8_t dir = (int8_t)(intptr_t)lv_event_get_user_data(e);
  settings().screen_off_min = option_step(SCREEN_OFF_MIN, SCREEN_OFF_COUNT,
                                          settings().screen_off_min, dir);
  refresh_off();
  settings_save();
}

void on_showcase(lv_event_t *) { nav_go(Screen::SettingsShowcase); }
void on_system(lv_event_t *) { nav_go(Screen::SettingsSystem); }

}  // namespace

lv_obj_t *settings_screen_create() {
  const AppSettings &st = settings();
  lv_obj_t *scr = ui_screen();
  set_header_version(ui_header(scr, tr(STR_TITLE_SETTINGS), on_back));

  lv_coord_t y = theme::HEADER_H;
  lv_obj_t *row = set_row(scr, y, kRowH);
  set_label(row, STR_SET_BRIGHTNESS);
  set_slider(row, 10, 100, st.brightness, on_brightness, on_save);

  row = set_row(scr, y += kRowH, kRowH);
  set_label(row, STR_SET_SOUND);
  set_switch(row, st.sound, on_sound);

  row = set_row(scr, y += kRowH, kRowH);
  set_label(row, STR_SET_OPEN_IN);
  s_views = set_segmented(row, kViews, 2, st.open_grid ? 1 : 0, on_view);

  row = set_row(scr, y += kRowH, kRowH);
  set_label(row, STR_SET_SCREEN_OFF);
  s_off = set_stepper(row, 52, on_off_step);
  refresh_off();

  char summary[48], interval[16];
  format_interval(st.showcase_interval_s, interval, sizeof(interval));
  snprintf(summary, sizeof(summary), "%s · %s", interval,
           tr(st.showcase_random ? STR_SC_ORDER_RAND_SHORT : STR_SC_ORDER_SEQ_SHORT));
  set_link_row(scr, y += kRowH, kRowH, &icon_monitor_16, STR_TITLE_SETTINGS_SHOWCASE, summary,
               on_showcase, true);
  y += kRowH;
  set_link_row(scr, y, 240 - y, &icon_globe_16, STR_SET_SYSTEM, i18n_lang_name(i18n_lang()),
               on_system, false);
  return scr;
}
