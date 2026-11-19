// 08 Busca (canvas: Busca.dc.html; regras na nota "Busca e Tipos").
//   cabeçalho 36 px: voltar, campo (lupa, texto, limpar)
//   faixa 28 px: até 3 sugestões (tocar abre a Ficha) + "N resultados"
//   teclado: QWERTY de 40 px ou 123; linha de baixo 123/ABC, espaço, Ver
// Cada tecla redesenha só o campo e a faixa de sugestões. A contagem varre as
// 1025 espécies com nome dobrado (logic/Search), poucos ms.
#include <cstdio>
#include <cstring>

#include "data/SpeciesRepo.h"
#include "data/StatusStore.h"
#include "i18n/i18n.h"
#include "logic/Search.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/icons/icons.h"
#include "ui/keyboard.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kHitsY = theme::HEADER_SEARCH_H;
constexpr lv_coord_t kHitsH = 28;
constexpr lv_coord_t kKbY = kHitsY + kHitsH;
constexpr uint8_t kMaxHits = 3;

char s_text[SEARCH_MAX + 1];
Screen s_back;
lv_obj_t *s_field;
lv_obj_t *s_hits_row;
lv_obj_t *s_count;

void on_back(lv_event_t *) { nav_go(s_back); }

void on_hit(lv_event_t *e) {
  CatalogState &cs = catalog_state();
  cs.selected = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
  cs.tab = DetailTab::Info;
  nav_go(Screen::Detail);
}

void refresh() {
  bool empty = s_text[0] == '\0';
  lv_label_set_text(s_field, empty ? tr(STR_SEARCH_PLACEHOLDER) : s_text);
  lv_obj_set_style_text_color(s_field, c(empty ? theme::MUTED : theme::TEXT), 0);

  lv_obj_clean(s_hits_row);
  SearchKey key;
  if (!search_key_make(s_text, &key)) {
    lv_label_set_text(s_count, "");
    return;
  }
  const DexStatus &st = status_store_get();
  uint16_t total = 0;
  uint16_t count = species_repo_count();
  for (uint16_t n = 1; n <= count; n++) {
    bool seen = st.is_seen(n);
    const char *name = species_repo_name(n);
    if (!search_matches(key, n, name, seen)) continue;
    if (total < kMaxHits) {
      char label[32];
      if (key.numeric) snprintf(label, sizeof(label), "#%03u %s", n, seen ? name : tr(STR_UNKNOWN_NAME));
      else snprintf(label, sizeof(label), "%s", name);
      lv_obj_t *chip = ui_box(s_hits_row);
      lv_obj_set_size(chip, LV_SIZE_CONTENT, 22);
      lv_obj_set_style_radius(chip, 11, 0);
      lv_obj_set_style_bg_color(chip, c(theme::SURFACE_2), 0);
      lv_obj_set_style_bg_color(chip, c(theme::LINE), LV_STATE_PRESSED);
      lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
      lv_obj_set_style_pad_hor(chip, 9, 0);
      lv_obj_set_ext_click_area(chip, 5);  // 22 + 2x5 = alvo de 32 px
      lv_obj_t *l = ui_label(chip, &font_12_600, theme::TEXT, label);
      lv_obj_set_style_max_width(l, 70, 0);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
      lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
      ui_on_click(chip, on_hit, (void *)(uintptr_t)n);
    }
    total++;
  }
  if (total == 1) lv_label_set_text(s_count, tr(STR_SEARCH_RESULT_ONE));
  else lv_label_set_text_fmt(s_count, tr(STR_SEARCH_RESULTS_FMT), total);
}

void type_char(char ch) {
  size_t len = strlen(s_text);
  if (len >= SEARCH_MAX) return;
  if (ch == ' ' && (len == 0 || s_text[len - 1] == ' ')) return;
  s_text[len] = ch;
  s_text[len + 1] = '\0';
  refresh();
}

void on_key(char ch) {
  if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
  type_char(ch);
}

void on_space() { type_char(' '); }

void on_backspace() {
  size_t len = strlen(s_text);
  if (len == 0) return;
  s_text[len - 1] = '\0';
  refresh();
}

void on_clear(lv_event_t *) {
  if (s_text[0] == '\0') return;
  s_text[0] = '\0';
  refresh();
}

void on_see() {
  // "Ver" vira filtro da Lista/Grade: só a busca, status volta pra Todos.
  CatalogState &cs = catalog_state();
  SearchKey key;
  cs.query.clear_refinements();
  cs.query.status = StatusFilter::All;
  if (search_key_make(s_text, &key)) snprintf(cs.query.text, sizeof(cs.query.text), "%s", s_text);
  cs.anchor = 1;
  catalog_invalidate();
  nav_go(cs.view);
}

}  // namespace

lv_obj_t *search_screen_create() {
  s_back = nav_previous();
  if (s_back != Screen::List && s_back != Screen::Grid) s_back = Screen::Menu;
  // Abre com a busca ativa, se houver (dá pra refinar o que já foi filtrado).
  snprintf(s_text, sizeof(s_text), "%s", catalog_state().query.text);

  lv_obj_t *scr = ui_screen();

  // Cabeçalho de 36 px (o único diferente dos 32: o campo precisa de 28).
  lv_obj_t *h = ui_box(scr);
  lv_obj_set_size(h, 320, theme::HEADER_SEARCH_H);
  lv_obj_set_style_bg_color(h, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(h, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(h, 1, 0);
  lv_obj_set_style_border_color(h, c(theme::LINE), 0);
  lv_obj_set_style_pad_right(h, 4, 0);
  lv_obj_set_style_pad_column(h, 2, 0);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *back = ui_box(h);
  lv_obj_set_size(back, 36, theme::HEADER_SEARCH_H - 1);
  lv_obj_center(ui_icon(back, &icon_back_20, theme::TEXT));
  ui_on_click(back, on_back);

  lv_obj_t *field = ui_box(h);
  lv_obj_set_height(field, 28);
  lv_obj_set_flex_grow(field, 1);
  lv_obj_set_style_radius(field, 6, 0);
  lv_obj_set_style_border_width(field, 1, 0);
  lv_obj_set_style_border_color(field, c(theme::ACCENT), 0);
  lv_obj_set_style_bg_color(field, c(theme::BG), 0);
  lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left(field, 8, 0);
  lv_obj_set_style_pad_right(field, 2, 0);
  lv_obj_set_style_pad_column(field, 6, 0);
  lv_obj_set_flex_flow(field, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(field, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_icon(field, &icon_search_14, theme::MUTED);
  s_field = ui_label(field, &font_14_600, theme::TEXT, "");
  lv_obj_set_flex_grow(s_field, 1);
  lv_label_set_long_mode(s_field, LV_LABEL_LONG_CLIP);
  lv_obj_t *clear = ui_box(field);
  lv_obj_set_size(clear, 24, 24);
  lv_obj_set_style_radius(clear, 12, 0);
  lv_obj_set_style_bg_color(clear, c(theme::SURFACE_2), 0);
  lv_obj_set_style_bg_color(clear, c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(clear, LV_OPA_COVER, 0);
  lv_obj_set_ext_click_area(clear, 6);
  lv_obj_center(ui_icon(clear, &icon_close_12, theme::MUTED));
  ui_on_click(clear, on_clear);

  // Sugestões + contagem.
  lv_obj_t *hits = ui_box(scr);
  lv_obj_set_pos(hits, 0, kHitsY);
  lv_obj_set_size(hits, 320, kHitsH);
  lv_obj_set_style_pad_hor(hits, 6, 0);
  lv_obj_set_style_pad_column(hits, 4, 0);
  lv_obj_set_flex_flow(hits, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hits, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  s_hits_row = ui_box(hits);
  lv_obj_set_size(s_hits_row, LV_SIZE_CONTENT, kHitsH);
  lv_obj_set_style_pad_column(s_hits_row, 4, 0);
  lv_obj_set_flex_flow(s_hits_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_hits_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  ui_header_spacer(hits);
  s_count = ui_label(hits, &font_12_500, theme::MUTED, "");

  // Teclado (ui/keyboard).
  lv_obj_t *kb = ui_box(scr);
  lv_obj_set_pos(kb, 0, kKbY);
  lv_obj_set_size(kb, 320, 240 - kKbY);
  keyboard_create(kb, KbLayout::Search, STR_SEARCH_SEE, {on_key, on_space, on_backspace, on_see});

  refresh();
  return scr;
}
