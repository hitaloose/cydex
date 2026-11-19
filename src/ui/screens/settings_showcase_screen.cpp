// 05b Ajustes › Modo vitrine (canvas: AjustesVitrine.dc.html).
//   cabeçalho: voltar, "Modo vitrine", ▶ (inicia a vitrine)
//   linhas de 36 px: Troca a cada, Ordem, Mostrar, Brilho, Iniciar ao ligar
//   resumo "312 espécies · ciclo completo em 5 h" no espaço que sobra
#include <cstdio>

#include "data/Settings.h"
#include "data/StatusStore.h"
#include "i18n/i18n.h"
#include "logic/Showcase.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/power.h"
#include "ui/settings_widgets.h"
#include "ui/theme.h"
#include "ui/widgets.h"

void format_interval(uint16_t seconds, char *out, size_t size);  // settings_screen.cpp

namespace {

constexpr lv_coord_t kRowH = 36;
const StrId kOrders[2] = {STR_SC_ORDER_SEQ, STR_SC_ORDER_RAND};
const StrId kSources[2] = {STR_FILTER_CAUGHT, STR_SC_SHOW_SEEN};

Stepper s_interval;
lv_obj_t *s_order;
lv_obj_t *s_source;
lv_obj_t *s_summary;

void refresh() {
  AppSettings &st = settings();
  char text[16];
  format_interval(st.showcase_interval_s, text, sizeof(text));
  uint8_t i = option_index(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, st.showcase_interval_s);
  set_stepper_update(s_interval, text, i == 0, i == SHOWCASE_INTERVAL_COUNT - 1);

  const DexStatus &ds = status_store_get();
  uint16_t total = st.showcase_all_seen ? ds.count_seen() : ds.count_caught();
  uint32_t minutes = showcase_cycle_minutes(total, st.showcase_interval_s);
  if (minutes >= 60)
    lv_label_set_text_fmt(s_summary, tr(STR_SC_SUMMARY_H_FMT), total, (unsigned)((minutes + 30) / 60));
  else
    lv_label_set_text_fmt(s_summary, tr(STR_SC_SUMMARY_MIN_FMT), total, (unsigned)minutes);
}

void on_back(lv_event_t *) { nav_go(Screen::Settings); }

void on_interval(lv_event_t *e) {
  int8_t dir = (int8_t)(intptr_t)lv_event_get_user_data(e);
  AppSettings &st = settings();
  st.showcase_interval_s =
      option_step(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, st.showcase_interval_s, dir);
  refresh();
  settings_save();
}

void on_order(lv_event_t *e) {
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  settings().showcase_random = i == 1;
  set_segmented_select(s_order, i);
  settings_save();
}

void on_source(lv_event_t *e) {
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  settings().showcase_all_seen = i == 1;
  set_segmented_select(s_source, i);
  refresh();
  settings_save();
}

// Prévia: enquanto arrasta, a tela fica no brilho da vitrine.
void on_brightness(lv_event_t *e) {
  uint8_t v = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
  settings().showcase_brightness = v;
  power_set_level(v);
}

void on_brightness_release(lv_event_t *) {
  power_use_normal_level();
  settings_save();
}

void on_start(lv_event_t *) { nav_go(Screen::Showcase); }

void on_boot(lv_event_t *e) {
  settings().showcase_on_boot = set_switch_get(lv_event_get_current_target(e));
  settings_save();
}

}  // namespace

lv_obj_t *settings_showcase_screen_create() {
  const AppSettings &st = settings();
  lv_obj_t *scr = ui_screen();
  lv_obj_t *h = ui_header(scr, tr(STR_TITLE_SETTINGS_SHOWCASE), on_back);
  ui_header_spacer(h);
  lv_obj_t *play = ui_header_button(h, &icon_play_18, on_start);
  lv_obj_set_style_img_recolor(lv_obj_get_child(play, 0), c(theme::ACCENT), 0);

  lv_coord_t y = theme::HEADER_H;
  lv_obj_t *row = set_row(scr, y, kRowH);
  set_label(row, STR_SC_INTERVAL);
  s_interval = set_stepper(row, 56, on_interval);

  row = set_row(scr, y += kRowH, kRowH);
  set_label(row, STR_SC_ORDER);
  s_order = set_segmented(row, kOrders, 2, st.showcase_random ? 1 : 0, on_order, 9);

  row = set_row(scr, y += kRowH, kRowH);
  set_label(row, STR_SC_SHOW);
  s_source = set_segmented(row, kSources, 2, st.showcase_all_seen ? 1 : 0, on_source, 9);

  row = set_row(scr, y += kRowH, kRowH);
  set_label(row, STR_SET_BRIGHTNESS);
  set_slider(row, 5, 100, st.showcase_brightness, on_brightness, on_brightness_release);
  y += kRowH;

  row = set_row(scr, y, kRowH);
  set_label(row, STR_SC_ON_BOOT);
  set_switch(row, st.showcase_on_boot, on_boot);
  y += kRowH;

  // Resumo centralizado no que sobra.
  s_summary = ui_label(scr, &font_12_500, theme::MUTED, "");
  lv_obj_set_width(s_summary, 300);
  lv_obj_set_style_text_align(s_summary, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_summary, LV_ALIGN_TOP_MID, 0, y + (240 - y - 15) / 2);

  refresh();
  return scr;
}
