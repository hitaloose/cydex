// 05a Ajustes › Tela e som (canvas: AjustesTela.dc.html).
//   Brilho, Brilho na vitrine (prévia enquanto arrasta), Apagar tela, Som ao tocar
#include "data/Settings.h"
#include "i18n/i18n.h"
#include "logic/Showcase.h"
#include "screens.h"
#include "ui/power.h"
#include "ui/settings_page.h"
#include "ui/settings_widgets.h"
#include "ui/widgets.h"

void format_screen_off(uint16_t minutes, char *out, size_t size);  // settings_screen.cpp

namespace {

uint8_t s_page;
Stepper s_off;

void on_back(lv_event_t *) { nav_go(Screen::Settings); }
void on_save(lv_event_t *) { settings_save(); }

void on_brightness(lv_event_t *e) {
  uint8_t v = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
  settings().brightness = v;
  power_set_level(v);
}

// Prévia: enquanto arrasta, a tela fica no brilho da vitrine.
void on_sc_brightness(lv_event_t *e) {
  uint8_t v = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
  settings().showcase_brightness = v;
  power_set_level(v);
}

void on_sc_brightness_release(lv_event_t *) {
  power_use_normal_level();
  settings_save();
}

void refresh_off() {
  uint16_t m = settings().screen_off_min;
  char text[16];
  format_screen_off(m, text, sizeof(text));
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

void on_sound(lv_event_t *e) {
  settings().sound = set_switch_get(lv_event_get_current_target(e));
  settings_save();
}

void row_brightness(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_row(body, y, h);
  set_label(row, STR_SET_BRIGHTNESS);
  set_slider(row, 10, 100, settings().brightness, on_brightness, on_save);
}

void row_sc_brightness(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_row(body, y, h);
  set_label(row, STR_SET_SC_BRIGHTNESS);
  set_slider(row, 5, 100, settings().showcase_brightness, on_sc_brightness,
             on_sc_brightness_release);
}

void row_off(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_row(body, y, h);
  set_label(row, STR_SET_SCREEN_OFF);
  s_off = set_stepper(row, 52, on_off_step);
  refresh_off();
}

void row_sound(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_row(body, y, h);
  set_label(row, STR_SET_SOUND);
  set_switch(row, settings().sound, on_sound);
}

const SettingsRow kRows[] = {
    {SETTINGS_ROW_H, row_brightness},
    {SETTINGS_ROW_H, row_sc_brightness},
    {SETTINGS_ROW_H, row_off},
    {SETTINGS_ROW_H, row_sound},
};

}  // namespace

lv_obj_t *settings_display_screen_create() {
  lv_obj_t *scr = ui_screen();
  ui_header(scr, tr(STR_SET_DISPLAY), on_back);
  settings_page(scr, kRows, sizeof(kRows) / sizeof(kRows[0]), &s_page);
  return scr;
}
