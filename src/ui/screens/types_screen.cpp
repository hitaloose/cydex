// 09 Filtro por tipo (canvas: Tipos.dc.html; regras na nota "Busca e Tipos").
//   cabeçalho: voltar, "Por tipo" + "até 2", Limpar
//   grade 4x5: 18 tipos + "Aplicar (n)" ocupando 2 colunas
// A escolha só vale ao tocar Aplicar; voltar descarta.
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr uint8_t kCols = 4, kRows = 5;
constexpr lv_coord_t kPad = 6, kGap = 4;
constexpr lv_coord_t kTop = theme::HEADER_H + kPad;
constexpr lv_coord_t kCellW = (320 - 2 * kPad - (kCols - 1) * kGap) / kCols;              // 74
constexpr lv_coord_t kCellH = (240 - kTop - kPad - (kRows - 1) * kGap) / kRows;           // 36

CatalogQuery s_sel;  // só os tipos importam aqui
Screen s_back;
lv_obj_t *s_btns[SPECIES_TYPE_COUNT];
lv_obj_t *s_badge;

void style_type(uint8_t i) {
  SpeciesType t = (SpeciesType)i;
  bool on = s_sel.has_type(t);
  lv_obj_t *b = s_btns[i];
  lv_obj_t *dot = lv_obj_get_child(b, 0);
  lv_obj_t *label = lv_obj_get_child(b, 1);
  lv_obj_set_style_bg_color(b, c(on ? species_type_color(t) : theme::SURFACE), 0);
  lv_obj_set_style_border_width(b, on ? 0 : 1, 0);
  lv_obj_set_style_text_font(label, on ? &font_12_700 : &font_12_600, 0);
  lv_obj_set_style_text_color(label, c(on ? theme::ON_ACCENT : theme::TEXT), 0);
  if (on) lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
}

void refresh() {
  for (uint8_t i = 0; i < SPECIES_TYPE_COUNT; i++) style_type(i);
  lv_label_set_text_fmt(s_badge, "%u", s_sel.type_count());
}

void on_type(lv_event_t *e) {
  s_sel.pick_type((SpeciesType)(uintptr_t)lv_event_get_user_data(e));
  refresh();
}

void on_clear(lv_event_t *) {
  if (s_sel.type_count() == 0) return;
  s_sel.type_a = s_sel.type_b = SpeciesType::None;
  refresh();
}

void on_apply(lv_event_t *) {
  // Tipos substituem a busca por texto; o filtro de status fica.
  CatalogState &cs = catalog_state();
  cs.query.text[0] = '\0';
  cs.query.type_a = s_sel.type_a;
  cs.query.type_b = s_sel.type_b;
  cs.anchor = 1;
  catalog_invalidate();
  nav_go(cs.view);
}

void on_back(lv_event_t *) { nav_go(s_back); }

lv_obj_t *cell(lv_obj_t *scr, uint8_t idx, uint8_t span) {
  uint8_t col = idx % kCols, row = idx / kCols;
  lv_obj_t *b = ui_box(scr);
  lv_obj_set_pos(b, kPad + col * (kCellW + kGap), kTop + row * (kCellH + kGap));
  lv_obj_set_size(b, span * kCellW + (span - 1) * kGap, kCellH);
  lv_obj_set_style_radius(b, 6, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(b, 3, 0);
  lv_obj_set_style_pad_column(b, 4, 0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return b;
}

}  // namespace

lv_obj_t *types_screen_create() {
  s_back = nav_previous();
  if (s_back != Screen::List && s_back != Screen::Grid) s_back = Screen::Menu;
  const CatalogQuery &q = catalog_state().query;
  s_sel = CatalogQuery();
  s_sel.type_a = q.type_a;
  s_sel.type_b = q.type_b;

  lv_obj_t *scr = ui_screen();
  lv_obj_t *h = ui_header(scr, tr(STR_TYPES_TITLE), on_back);
  lv_obj_t *hint = ui_label(h, &font_12_500, theme::MUTED, tr(STR_TYPES_UP_TO));
  lv_obj_set_style_pad_left(hint, 6, 0);
  ui_header_spacer(h);
  lv_obj_t *clear = ui_box(h);
  lv_obj_set_size(clear, LV_SIZE_CONTENT, theme::HEADER_H - 1);
  lv_obj_set_style_pad_hor(clear, 12, 0);
  lv_obj_t *cl = ui_label(clear, &font_12_600, theme::MUTED, tr(STR_TYPES_CLEAR));
  lv_obj_align(cl, LV_ALIGN_LEFT_MID, 0, 0);
  ui_on_click(clear, on_clear);

  for (uint8_t i = 0; i < SPECIES_TYPE_COUNT; i++) {
    SpeciesType t = (SpeciesType)i;
    lv_obj_t *b = cell(scr, i, 1);
    lv_obj_set_style_border_color(b, c(theme::LINE), 0);
    lv_obj_t *dot = ui_box(b);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, 4, 0);
    lv_obj_set_style_bg_color(dot, c(species_type_color(t)), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_t *l = ui_label(b, &font_12_600, theme::TEXT, species_type_name(t));
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_max_width(l, kCellW - 6 - 12, 0);
    ui_on_click(b, on_type, (void *)(uintptr_t)i);
    s_btns[i] = b;
  }

  lv_obj_t *apply = cell(scr, SPECIES_TYPE_COUNT, 2);
  lv_obj_set_style_bg_color(apply, c(theme::ACCENT), 0);
  lv_obj_set_style_bg_color(apply, c(theme::TEXT), LV_STATE_PRESSED);
  lv_obj_set_style_pad_column(apply, 6, 0);
  ui_label(apply, &font_14_700, theme::ON_ACCENT, tr(STR_TYPES_APPLY));
  lv_obj_t *badge = ui_box(apply);
  lv_obj_set_size(badge, LV_SIZE_CONTENT, 18);
  lv_obj_set_style_min_width(badge, 18, 0);
  lv_obj_set_style_radius(badge, 9, 0);
  lv_obj_set_style_bg_color(badge, c(theme::BG), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(badge, 5, 0);
  s_badge = ui_label(badge, &font_12_700, theme::ACCENT, "0");
  lv_obj_center(s_badge);
  ui_on_click(apply, on_apply);

  refresh();
  return scr;
}
