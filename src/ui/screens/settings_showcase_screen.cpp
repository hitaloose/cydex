// 05b Ajustes › Modo vitrine (canvas: AjustesVitrine.dc.html).
//   cabeçalho: voltar, "Modo vitrine", ▶ (inicia a vitrine)
//   Troca a cada, Ordem, Mostrar › (abre a Escolha: capturados, vistos, todos)
//   resumo "312 espécies · ciclo completo em 5 h" no espaço que sobra
#include <cstdio>

#include "data/Settings.h"
#include "data/SpeciesRepo.h"
#include "data/StatusStore.h"
#include "i18n/i18n.h"
#include "logic/Showcase.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/settings_page.h"
#include "ui/settings_widgets.h"
#include "ui/theme.h"
#include "ui/widgets.h"

void format_interval(uint16_t seconds, char *out, size_t size);  // settings_screen.cpp

namespace {

const StrId kOrders[2] = {STR_SC_ORDER_SEQ, STR_SC_ORDER_RAND};
const StrId kSources[SHOWCASE_SOURCE_COUNT] = {STR_FILTER_CAUGHT, STR_SC_SHOW_SEEN,
                                               STR_SC_SHOW_ALL};

uint8_t s_page;
Stepper s_interval;
lv_obj_t *s_order;
lv_obj_t *s_summary;

uint16_t source_total(uint8_t src) {
  const DexStatus &ds = status_store_get();
  switch ((ShowcaseSource)src) {
    case ShowcaseSource::Seen: return ds.count_seen();
    case ShowcaseSource::All: return species_repo_count();
    default: return ds.count_caught();
  }
}

void refresh() {
  AppSettings &st = settings();
  char text[16];
  format_interval(st.showcase_interval_s, text, sizeof(text));
  uint8_t i = option_index(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, st.showcase_interval_s);
  set_stepper_update(s_interval, text, i == 0, i == SHOWCASE_INTERVAL_COUNT - 1);

  if (!s_summary) return;
  uint16_t total = source_total(st.showcase_source);
  uint32_t minutes = showcase_cycle_minutes(total, st.showcase_interval_s);
  if (minutes >= 60)
    lv_label_set_text_fmt(s_summary, tr(STR_SC_SUMMARY_H_FMT), total, (unsigned)((minutes + 30) / 60));
  else
    lv_label_set_text_fmt(s_summary, tr(STR_SC_SUMMARY_MIN_FMT), total, (unsigned)minutes);
}

void on_back(lv_event_t *) { nav_go(Screen::Settings); }
void on_start(lv_event_t *) { nav_go(Screen::Showcase); }

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

// Escolha (05g) do "Mostrar".
const char *source_label(uint8_t i) { return tr(kSources[i]); }
const char *source_hint(uint8_t i) {
  static char buf[8];
  snprintf(buf, sizeof(buf), "%u", source_total(i));
  return buf;
}
void source_pick(uint8_t i) {
  settings().showcase_source = i;
  settings_save();
}

void on_source(lv_event_t *) {
  ChoiceSpec spec;
  spec.title = STR_SC_SHOW;
  spec.count = SHOWCASE_SOURCE_COUNT;
  spec.selected = settings().showcase_source;
  spec.label = source_label;
  spec.hint = source_hint;
  spec.pick = source_pick;
  spec.back = Screen::SettingsShowcase;
  settings_choice_open(spec);
}

void row_interval(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_row(body, y, h);
  set_label(row, STR_SC_INTERVAL);
  s_interval = set_stepper(row, 56, on_interval);
}

void row_order(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_row(body, y, h);
  set_label(row, STR_SC_ORDER);
  s_order = set_segmented(row, kOrders, 2, settings().showcase_random ? 1 : 0, on_order, 9);
}

void row_source(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  set_link_row(body, y, h, &icon_eye_16, STR_SC_SHOW, source_label(settings().showcase_source),
               on_source, true);
}

const SettingsRow kRows[] = {
    {SETTINGS_ROW_H, row_interval},
    {SETTINGS_ROW_H, row_order},
    {SETTINGS_ROW_H, row_source},
};

void on_delete(lv_event_t *) { s_summary = nullptr; }

}  // namespace

lv_obj_t *settings_showcase_screen_create() {
  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);
  lv_obj_t *h = ui_header(scr, tr(STR_TITLE_SETTINGS_SHOWCASE), on_back);
  ui_header_spacer(h);
  lv_obj_t *play = ui_header_button(h, &icon_play_18, on_start);
  lv_obj_set_style_img_recolor(lv_obj_get_child(play, 0), c(theme::ACCENT), 0);

  s_summary = nullptr;
  lv_coord_t y = settings_page(scr, kRows, sizeof(kRows) / sizeof(kRows[0]), &s_page);

  // Resumo centralizado no que sobra (com as 3 linhas, a página é uma só).
  s_summary = ui_label(scr, &font_12_500, theme::MUTED, "");
  lv_obj_set_width(s_summary, 300);
  lv_obj_set_style_text_align(s_summary, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_summary, LV_ALIGN_TOP_MID, 0, y + (240 - y - 15) / 2);

  refresh();
  return scr;
}
