// 07 Grade (canvas: Grade.dc.html). 5x2 = 10 por página, sprite 48x48.
//   desconhecido: caixa tracejada com "?"; capturado: selo com check no canto.
//   Sprites da página carregados de uma vez; enquanto segura ◀ ▶ só os
//   números trocam e os sprites entram quando soltar.
#include <cstdio>

#include "data/SpriteLoader.h"
#include "data/StatusStore.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/icons/icons.h"
#include "ui/pager_bar.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "util/log.h"

namespace {

constexpr uint8_t kCols = 5, kRowsN = 2, kCells = kCols * kRowsN;
constexpr lv_coord_t kPad = 6, kGap = 4;
constexpr lv_coord_t kAreaY = theme::HEADER_H;
constexpr lv_coord_t kCellW = (320 - 2 * kPad - (kCols - 1) * kGap) / kCols;              // 58
constexpr lv_coord_t kCellH = (240 - kAreaY - theme::PAGER_H - 2 * kPad - kGap) / kRowsN;  // 77
constexpr lv_coord_t kSpriteY = (kCellH - (48 + 4 + 14)) / 2;                                // 5

struct Cell {
  lv_obj_t *root;
  lv_obj_t *sprite;  // lv_img: sprite ou caixa tracejada
  lv_obj_t *glyph;   // "?"
  lv_obj_t *num;
  lv_obj_t *badge;   // selo de capturado
};

Cell s_cells[kCells];
Sprite s_sprites[kCells];
lv_obj_t *s_empty;
PagerBar s_pager;

void free_sprites() {
  for (Sprite &s : s_sprites) sprite_free(&s);
}

void show_dash_box(Cell &cell, bool question) {
  lv_img_set_src(cell.sprite, &icon_dash_box_48);
  lv_obj_set_style_img_recolor_opa(cell.sprite, LV_OPA_COVER, 0);
  lv_obj_set_style_img_recolor(cell.sprite, c(theme::FAINT_2), 0);
  if (question) lv_obj_clear_flag(cell.glyph, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(cell.glyph, LV_OBJ_FLAG_HIDDEN);
}

void render(bool holding) {
  CatalogState &cs = catalog_state();
  uint16_t len;
  const uint16_t *list = catalog_list(&len);
  const DexStatus &st = status_store_get();
  uint16_t first = s_pager.pager.first_index();
  uint16_t count = s_pager.pager.items_on_page();
  if (count) cs.anchor = list[first];

  // Libera a página anterior antes de alocar a nova (heap sem bloco grande).
  free_sprites();
  uint32_t t0 = lv_tick_get();

  for (uint8_t i = 0; i < kCells; i++) {
    Cell &cell = s_cells[i];
    if (i >= count) {
      lv_obj_add_flag(cell.root, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(cell.root, LV_OBJ_FLAG_HIDDEN);
    uint16_t n = list[first + i];
    lv_obj_set_user_data(cell.root, (void *)(uintptr_t)n);
    SpeciesState state = st.state(n);
    bool unknown = state == SpeciesState::Unknown;

    lv_label_set_text_fmt(cell.num, "%03u", n);
    lv_obj_set_style_text_color(cell.num, c(unknown ? theme::MUTED : theme::TEXT), 0);
    if (state == SpeciesState::Caught) lv_obj_clear_flag(cell.badge, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(cell.badge, LV_OBJ_FLAG_HIDDEN);

    if (unknown) {
      show_dash_box(cell, true);
    } else if (!holding && sprite_load(n, SpriteSize::S48, &s_sprites[i])) {
      lv_obj_set_style_img_recolor_opa(cell.sprite, LV_OPA_TRANSP, 0);
      lv_img_set_src(cell.sprite, &s_sprites[i].dsc);
      lv_obj_add_flag(cell.glyph, LV_OBJ_FLAG_HIDDEN);
    } else {
      show_dash_box(cell, false);
    }
  }
  if (!holding && count) {
    LOG_D("grid", "página %u: %u sprites em %ums, maior_bloco=%u", s_pager.pager.page, count,
          lv_tick_elaps(t0), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  }

  if (len == 0) lv_obj_clear_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);

  char range[24];
  if (count) snprintf(range, sizeof(range), "#%03u–%03u", list[first], list[first + count - 1]);
  else range[0] = '\0';
  pager_bar_refresh(&s_pager, range);
}

void on_cell(lv_event_t *e) {
  catalog_state().selected = (uint16_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
  catalog_state().tab = DetailTab::Info;
  nav_go(Screen::Detail);
}

void on_delete(lv_event_t *) { free_sprites(); }
void on_back(lv_event_t *) { nav_go(Screen::Menu); }
void on_search(lv_event_t *) { nav_go(Screen::Search); }
void on_types(lv_event_t *) { nav_go(Screen::Types); }
void on_list(lv_event_t *) { nav_go(Screen::List); }
void on_center() { nav_go(Screen::GoTo); }

}  // namespace

lv_obj_t *grid_screen_create() {
  CatalogState &cs = catalog_state();
  cs.view = Screen::Grid;
  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);

  lv_obj_t *h = catalog_header(scr, on_back);
  ui_header_spacer(h);
  ui_header_button(h, &icon_search_18, on_search);
  ui_header_button(h, &icon_tag_18, on_types);
  ui_header_button(h, &icon_list_18, on_list);

  for (uint8_t i = 0; i < kCells; i++) {
    Cell &cell = s_cells[i];
    uint8_t col = i % kCols, row = i / kCols;
    cell.root = ui_box(scr);
    // Coluna com 1 px a mais nas últimas pra fechar os 308 px (58,4 cada).
    lv_coord_t x = kPad + (col * (320 - 2 * kPad + kGap)) / kCols;
    lv_obj_set_pos(cell.root, x, kAreaY + kPad + row * (kCellH + kGap));
    lv_obj_set_size(cell.root, kCellW, kCellH);
    lv_obj_set_style_radius(cell.root, 6, 0);
    lv_obj_set_style_bg_color(cell.root, c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(cell.root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cell.root, c(theme::SURFACE_2), LV_STATE_PRESSED);
    ui_on_click(cell.root, on_cell);

    cell.sprite = lv_img_create(cell.root);
    lv_obj_align(cell.sprite, LV_ALIGN_TOP_MID, 0, kSpriteY);
    cell.glyph = ui_label(cell.root, &font_16_700, theme::FAINT, "?");
    lv_obj_align(cell.glyph, LV_ALIGN_TOP_MID, 0, kSpriteY + 15);
    cell.num = ui_label(cell.root, &font_12_600, theme::TEXT, "");
    lv_obj_align(cell.num, LV_ALIGN_TOP_MID, 0, kSpriteY + 48 + 4);

    cell.badge = ui_box(cell.root);
    lv_obj_set_size(cell.badge, 12, 12);
    lv_obj_align(cell.badge, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_style_radius(cell.badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cell.badge, c(theme::ACCENT), 0);
    lv_obj_set_style_bg_opa(cell.badge, LV_OPA_COVER, 0);
    lv_obj_center(ui_icon(cell.badge, &icon_check_12, theme::ON_ACCENT));
  }

  s_empty = ui_label(scr, &font_14_600, theme::MUTED, tr(STR_CATALOG_EMPTY));
  lv_obj_center(s_empty);

  s_pager.on_page = render;
  s_pager.on_center = on_center;
  pager_bar_create(&s_pager, scr);

  uint16_t len;
  const uint16_t *list = catalog_list(&len);
  s_pager.pager.reset(len, kCells);
  s_pager.pager.go_to_index(catalog_index_at_or_after(list, len, cs.anchor));
  render(false);
  return scr;
}
