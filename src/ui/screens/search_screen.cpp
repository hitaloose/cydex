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
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kHitsY = theme::HEADER_SEARCH_H;
constexpr lv_coord_t kHitsH = 28;
constexpr lv_coord_t kKbY = kHitsY + kHitsH;
constexpr uint8_t kMaxHits = 3;

// Larguras relativas do canvas (flex) x5, porque o flex_grow do LVGL é inteiro.
constexpr uint8_t kKeyGrow = 5, kBackspaceGrow = 8, kModeGrow = 8, kSpaceGrow = 25, kSeeGrow = 12;

const char *const kLetters[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM-"};
// Os nomes só usam espaço ' - . : (♀/♂ viram " F"/" M" no build_sd.py) e a
// busca ignora pontuação; o 123 fica com dígitos e esses sinais.
const char *const kDigits[3] = {"1234567890", "-.':", ""};

char s_text[SEARCH_MAX + 1];
bool s_digits_mode;
Screen s_back;
lv_obj_t *s_field;
lv_obj_t *s_hits_row;
lv_obj_t *s_count;
lv_obj_t *s_keyboard;

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

void on_key(lv_event_t *e) {
  char ch = (char)(uintptr_t)lv_event_get_user_data(e);
  if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
  type_char(ch);
}

void on_space(lv_event_t *) { type_char(' '); }

void on_backspace(lv_event_t *) {
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

void on_see(lv_event_t *) {
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

void build_keyboard();
void on_mode(lv_event_t *) {
  s_digits_mode = !s_digits_mode;
  build_keyboard();
}

lv_obj_t *key(lv_obj_t *row, uint8_t grow, uint32_t bg) {
  lv_obj_t *k = ui_box(row);
  lv_obj_set_height(k, 40);
  lv_obj_set_flex_grow(k, grow);
  lv_obj_set_style_radius(k, 4, 0);
  lv_obj_set_style_bg_color(k, c(bg), 0);
  lv_obj_set_style_bg_color(k, c(theme::FAINT_2), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
  return k;
}

lv_obj_t *key_row(lv_obj_t *kb, lv_coord_t pad_hor) {
  lv_obj_t *row = ui_box(kb);
  lv_obj_set_size(row, LV_PCT(100), 40);
  lv_obj_set_style_pad_hor(row, pad_hor, 0);
  lv_obj_set_style_pad_column(row, 2, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  return row;
}

void build_keyboard() {
  lv_obj_clean(s_keyboard);
  const char *const *rows = s_digits_mode ? kDigits : kLetters;
  for (uint8_t r = 0; r < 3; r++) {
    // 2a linha do QWERTY recuada 15 px dos dois lados (canvas).
    lv_obj_t *row = key_row(s_keyboard, (r == 1 && !s_digits_mode) ? 15 : 0);
    for (const char *p = rows[r]; *p; p++) {
      lv_obj_t *k = key(row, kKeyGrow, theme::SURFACE_2);
      char text[2] = {*p, '\0'};
      lv_obj_center(ui_label(k, &font_14_600, theme::TEXT, text));
      ui_on_click(k, on_key, (void *)(uintptr_t)(unsigned char)*p);
    }
    if (r == 2) {
      if (rows[r][0] == '\0') {
        // Linha só com o apagar: espaçador mantém a tecla do tamanho de sempre.
        lv_obj_t *sp = ui_box(row);
        lv_obj_set_height(sp, 1);
        lv_obj_set_flex_grow(sp, 8 * kKeyGrow);
      }
      lv_obj_t *bs = key(row, kBackspaceGrow, theme::LINE);
      lv_obj_center(ui_icon(bs, &icon_backspace_18, theme::TEXT));
      ui_on_click(bs, on_backspace);
    }
  }
  lv_obj_t *row = key_row(s_keyboard, 0);
  lv_obj_t *mode = key(row, kModeGrow, theme::LINE);
  lv_obj_center(ui_label(mode, &font_12_700, theme::TEXT,
                         tr(s_digits_mode ? STR_SEARCH_MODE_ABC : STR_SEARCH_MODE_NUM)));
  ui_on_click(mode, on_mode);
  lv_obj_t *space = key(row, kSpaceGrow, theme::SURFACE_2);
  lv_obj_center(ui_label(space, &font_12_600, theme::MUTED, tr(STR_SEARCH_SPACE)));
  ui_on_click(space, on_space);
  lv_obj_t *see = key(row, kSeeGrow, theme::ACCENT);
  lv_obj_set_style_bg_color(see, c(theme::TEXT), LV_STATE_PRESSED);
  lv_obj_center(ui_label(see, &font_14_700, theme::ON_ACCENT, tr(STR_SEARCH_SEE)));
  ui_on_click(see, on_see);
}

}  // namespace

lv_obj_t *search_screen_create() {
  s_back = nav_previous();
  if (s_back != Screen::List && s_back != Screen::Grid) s_back = Screen::Menu;
  // Abre com a busca ativa, se houver (dá pra refinar o que já foi filtrado).
  snprintf(s_text, sizeof(s_text), "%s", catalog_state().query.text);
  s_digits_mode = false;

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

  // Teclado.
  s_keyboard = ui_box(scr);
  lv_obj_set_pos(s_keyboard, 0, kKbY);
  lv_obj_set_size(s_keyboard, 320, 240 - kKbY);
  lv_obj_set_style_bg_color(s_keyboard, c(theme::SURFACE_DIM), 0);
  lv_obj_set_style_bg_opa(s_keyboard, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(s_keyboard, 2, 0);
  lv_obj_set_style_pad_row(s_keyboard, 2, 0);
  lv_obj_set_flex_flow(s_keyboard, LV_FLEX_FLOW_COLUMN);
  build_keyboard();

  refresh();
  return scr;
}
