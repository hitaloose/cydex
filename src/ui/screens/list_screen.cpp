// 06 Lista (canvas: Lista.dc.html).
//   cabeçalho: voltar, "Nacional", buscar, ver em grade
//   filtro Todos / Capturados / Faltando (34 px)
//   4 linhas de 34 px: marca de status, #nº, nome (??? se desconhecido), bolinhas de tipo
//   rodapé de paginação (toque no meio = Ir para)
#include <cstdio>

#include "data/SpeciesRepo.h"
#include "data/StatusStore.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/icons/icons.h"
#include "ui/pager_bar.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr uint8_t kRows = 4;
constexpr lv_coord_t kFilterY = theme::HEADER_H;
constexpr lv_coord_t kFilterH = 34;
constexpr lv_coord_t kRowsY = kFilterY + kFilterH;

struct Row {
  lv_obj_t *root;
  lv_obj_t *mark;
  lv_obj_t *num;
  lv_obj_t *name;
  lv_obj_t *dots[2];
};

Row s_rows[kRows];
lv_obj_t *s_filters[3];
lv_obj_t *s_empty;
PagerBar s_pager;

const StatusFilter kFilterIds[3] = {StatusFilter::All, StatusFilter::Caught, StatusFilter::Missing};
const StrId kFilterLabels[3] = {STR_FILTER_ALL, STR_FILTER_CAUGHT, STR_FILTER_MISSING};

void style_filter(lv_obj_t *b, bool on) {
  lv_obj_t *l = lv_obj_get_child(b, 0);
  lv_obj_set_style_bg_color(b, c(on ? theme::ACCENT : theme::SURFACE), 0);
  lv_obj_set_style_border_width(b, on ? 0 : 1, 0);
  lv_obj_set_style_text_font(l, on ? &font_12_700 : &font_12_600, 0);
  lv_obj_set_style_text_color(l, c(on ? theme::ON_ACCENT : theme::TEXT), 0);
}

void render(bool) {
  CatalogState &cs = catalog_state();
  uint16_t len;
  const uint16_t *list = catalog_list(&len);
  const DexStatus &st = status_store_get();
  uint16_t first = s_pager.pager.first_index();
  uint16_t count = s_pager.pager.items_on_page();
  if (count) cs.anchor = list[first];

  for (uint8_t i = 0; i < kRows; i++) {
    Row &r = s_rows[i];
    if (i >= count) {
      lv_obj_add_flag(r.root, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(r.root, LV_OBJ_FLAG_HIDDEN);
    uint16_t n = list[first + i];
    lv_obj_set_user_data(r.root, (void *)(uintptr_t)n);
    SpeciesState state = st.state(n);
    ui_status_mark_set(r.mark, state);
    lv_label_set_text_fmt(r.num, "#%03u", n);

    bool unknown = state == SpeciesState::Unknown;
    lv_label_set_text(r.name, unknown ? tr(STR_UNKNOWN_NAME) : species_repo_name(n));
    lv_obj_set_style_text_color(r.name, c(unknown ? theme::MUTED : theme::TEXT), 0);

    SpeciesType t[2];
    species_repo_types(n, &t[0], &t[1]);
    for (int k = 0; k < 2; k++) {
      if (unknown || t[k] == SpeciesType::None) {
        lv_obj_add_flag(r.dots[k], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(r.dots[k], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(r.dots[k], c(species_type_color(t[k])), 0);
      }
    }
  }

  if (len == 0) lv_obj_clear_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);

  char range[24];
  if (count) snprintf(range, sizeof(range), "#%03u–%03u", list[first], list[first + count - 1]);
  else range[0] = '\0';
  pager_bar_refresh(&s_pager, range);
}

void reload_list() {
  uint16_t len;
  const uint16_t *list = catalog_list(&len);
  s_pager.pager.reset(len, kRows);
  s_pager.pager.go_to_index(catalog_index_at_or_after(list, len, catalog_state().anchor));
  render(false);
}

void on_filter(lv_event_t *e) {
  uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  CatalogState &cs = catalog_state();
  if (cs.query.status == kFilterIds[idx]) return;
  cs.query.status = kFilterIds[idx];
  catalog_invalidate();
  for (uint8_t i = 0; i < 3; i++) style_filter(s_filters[i], i == idx);
  cs.anchor = 1;  // como no canvas: filtro novo volta pra página 1
  reload_list();
}

void on_row(lv_event_t *e) {
  uint16_t n = (uint16_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
  catalog_state().selected = n;
  catalog_state().tab = DetailTab::Info;
  nav_go(Screen::Detail);
}

void on_back(lv_event_t *) { nav_go(Screen::Menu); }
void on_search(lv_event_t *) { nav_go(Screen::Search); }
void on_types(lv_event_t *) { nav_go(Screen::Types); }
void on_grid(lv_event_t *) { nav_go(Screen::Grid); }
void on_center() { nav_go(Screen::GoTo); }

}  // namespace

lv_obj_t *list_screen_create() {
  CatalogState &cs = catalog_state();
  cs.view = Screen::List;
  lv_obj_t *scr = ui_screen();

  lv_obj_t *h = catalog_header(scr, on_back);
  ui_header_spacer(h);
  ui_header_button(h, &icon_search_18, on_search);
  ui_header_button(h, &icon_tag_18, on_types);
  ui_header_button(h, &icon_grid_18, on_grid);

  // Filtros
  lv_obj_t *fr = ui_box(scr);
  lv_obj_set_pos(fr, 0, kFilterY);
  lv_obj_set_size(fr, LV_PCT(100), kFilterH);
  lv_obj_set_style_pad_hor(fr, 8, 0);
  lv_obj_set_style_pad_ver(fr, 4, 0);
  lv_obj_set_style_pad_column(fr, 4, 0);
  lv_obj_set_flex_flow(fr, LV_FLEX_FLOW_ROW);
  for (uint8_t i = 0; i < 3; i++) {
    lv_obj_t *b = ui_box(fr);
    lv_obj_set_height(b, 26);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, c(theme::LINE), 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, tr(kFilterLabels[i]));
    lv_obj_center(l);
    // A faixa tem 26 px de altura; a área de toque vai até a borda da faixa (34 px).
    lv_obj_set_ext_click_area(b, 4);
    ui_on_click(b, on_filter, (void *)(uintptr_t)i);
    s_filters[i] = b;
    style_filter(b, cs.query.status == kFilterIds[i]);
  }

  // Divisória acima da 1a linha + linhas
  lv_obj_t *list = ui_box(scr);
  lv_obj_set_pos(list, 0, kRowsY);
  lv_obj_set_size(list, LV_PCT(100), 240 - kRowsY - theme::PAGER_H);
  lv_obj_set_style_border_side(list, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(list, 1, 0);
  lv_obj_set_style_border_color(list, c(theme::ROW_DIVIDER), 0);

  for (uint8_t i = 0; i < kRows; i++) {
    Row &r = s_rows[i];
    r.root = ui_box(list);
    lv_obj_set_pos(r.root, 0, 1 + i * theme::ROW_H);
    lv_obj_set_size(r.root, LV_PCT(100), theme::ROW_H);
    lv_obj_set_style_border_side(r.root, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r.root, 1, 0);
    lv_obj_set_style_border_color(r.root, c(theme::ROW_DIVIDER), 0);
    lv_obj_set_style_pad_left(r.root, 10, 0);
    lv_obj_set_style_pad_right(r.root, 12, 0);
    lv_obj_set_style_pad_column(r.root, 8, 0);
    lv_obj_set_flex_flow(r.root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r.root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // Toque destaca a linha (feedback sem animação).
    lv_obj_set_style_bg_color(r.root, c(theme::SURFACE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r.root, LV_OPA_COVER, LV_STATE_PRESSED);
    ui_on_click(r.root, on_row);

    r.mark = ui_status_mark(r.root);
    r.num = ui_label(r.root, &font_12_600, theme::MUTED, "");
    lv_obj_set_width(r.num, 38);
    r.name = ui_label(r.root, &font_14_600, theme::TEXT, "");
    lv_obj_set_flex_grow(r.name, 1);
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);

    lv_obj_t *dots = ui_box(r.root);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, 8);
    lv_obj_set_style_pad_column(dots, 3, 0);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    for (int k = 0; k < 2; k++) {
      lv_obj_t *d = ui_box(dots);
      lv_obj_set_size(d, 8, 8);
      lv_obj_set_style_radius(d, 4, 0);
      lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
      r.dots[k] = d;
    }
  }

  s_empty = ui_label(list, &font_14_600, theme::MUTED, tr(STR_CATALOG_EMPTY));
  lv_obj_center(s_empty);

  s_pager.on_page = render;
  s_pager.on_center = on_center;
  pager_bar_create(&s_pager, scr);

  reload_list();
  return scr;
}
