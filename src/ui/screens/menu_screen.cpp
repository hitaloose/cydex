// 03 Menu (canvas: Menu.dc.html).
//   cabeçalho 32 px: logo, "CYDex", pílula "Vitrine"
//   faixa de capturados (N / 1025 + barra)
//   grade: tile grande Lista (118 px, 2 linhas) + Buscar, Tipos, Progresso, Ajustes
#include <cstdio>

#include "data/SpeciesRepo.h"
#include "data/Settings.h"
#include "data/StatusStore.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/nav.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

// Geometria tirada do canvas (320x240).
constexpr lv_coord_t kSectionY = theme::HEADER_H;  // faixa de capturados
constexpr lv_coord_t kGridX = 8;
constexpr lv_coord_t kGridY = 76;
constexpr lv_coord_t kGridW = 320 - 2 * 8;
constexpr lv_coord_t kGridH = 240 - kGridY - 8;
constexpr lv_coord_t kGap = 6;
constexpr lv_coord_t kBigW = 118;
constexpr lv_coord_t kSmallW = (kGridW - kBigW - 2 * kGap) / 2;
constexpr lv_coord_t kSmallH = (kGridH - kGap) / 2;

void go(lv_event_t *e) { nav_go((Screen)(uintptr_t)lv_event_get_user_data(e)); }

lv_obj_t *tile(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
               bool primary, Screen target) {
  lv_obj_t *t = ui_box(parent);
  lv_obj_set_pos(t, x, y);
  lv_obj_set_size(t, w, h);
  lv_obj_set_style_radius(t, 10, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  if (primary) {
    lv_obj_set_style_bg_color(t, c(theme::ACCENT), 0);
    lv_obj_set_style_pad_all(t, 10, 0);
  } else {
    lv_obj_set_style_bg_color(t, c(theme::SURFACE), 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_border_color(t, c(theme::LINE), 0);
    lv_obj_set_style_pad_all(t, 7, 0);  // 8 do canvas - 1 da borda
  }
  ui_on_click(t, go, (void *)(uintptr_t)target);
  return t;
}

void small_tile(lv_obj_t *parent, int col, int row, const lv_img_dsc_t *icon, StrId text,
                Screen target) {
  lv_coord_t x = kGridX + kBigW + kGap + col * (kSmallW + kGap);
  lv_coord_t y = kGridY + row * (kSmallH + kGap);
  lv_obj_t *t = tile(parent, x, y, kSmallW, kSmallH, false, target);
  lv_obj_t *i = ui_icon(t, icon, theme::ACCENT);
  lv_obj_align(i, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *l = ui_label(t, &font_14_600, theme::TEXT, tr(text));
  lv_obj_align(l, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void header(lv_obj_t *scr) {
  lv_obj_t *h = ui_box(scr);
  lv_obj_set_size(h, LV_PCT(100), theme::HEADER_H);
  lv_obj_set_style_bg_color(h, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(h, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(h, 1, 0);
  lv_obj_set_style_border_color(h, c(theme::LINE), 0);
  lv_obj_set_style_pad_hor(h, 10, 0);
  lv_obj_set_style_pad_column(h, 8, 0);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  ui_logo(h, 18);
  lv_obj_t *title = ui_label(h, &font_16_700, theme::TEXT, "CYDex");
  lv_obj_set_flex_grow(title, 1);

  // Sem indicador de SD: sem cartão o aparelho nem passa do Boot.
  lv_obj_t *pill = ui_box(h);
  lv_obj_set_size(pill, LV_SIZE_CONTENT, 24);
  lv_obj_set_style_pad_hor(pill, 7, 0);
  lv_obj_set_style_radius(pill, 12, 0);
  lv_obj_set_style_bg_color(pill, c(theme::SURFACE_2), 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(pill, 1, 0);
  lv_obj_set_style_border_color(pill, c(theme::LINE), 0);
  lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(pill, 5, 0);
  ui_icon(pill, &icon_monitor_14, theme::ACCENT);
  ui_label(pill, &font_12_600, theme::TEXT, tr(STR_TITLE_SHOWCASE));
  // A pílula tem 24 px de altura; a área de toque cresce pra 36 (regra ≥ 32).
  lv_obj_set_ext_click_area(pill, 6);
  ui_on_click(pill, go, (void *)(uintptr_t)Screen::Showcase);
}

void caught_section(lv_obj_t *scr) {
  const DexStatus &st = status_store_get();
  uint16_t total = species_repo_count();
  uint16_t caught = st.count_caught();

  lv_obj_t *sec = ui_box(scr);
  lv_obj_set_pos(sec, 10, kSectionY + 8);
  lv_obj_set_size(sec, 300, 28);

  lv_obj_t *label = ui_label(sec, &font_12_600, theme::MUTED, tr(STR_MENU_CAUGHT));
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 3);

  char buf[16];
  snprintf(buf, sizeof(buf), "/ %u", total);
  lv_obj_t *of = ui_label(sec, &font_12_500, theme::MUTED, buf);
  lv_obj_align(of, LV_ALIGN_TOP_RIGHT, 0, 3);
  snprintf(buf, sizeof(buf), "%u", caught);
  lv_obj_t *num = ui_label(sec, &font_14_700, theme::TEXT, buf);
  lv_obj_align_to(num, of, LV_ALIGN_OUT_LEFT_BOTTOM, -6, 1);

  lv_obj_t *bar = ui_progress(sec, 300, 6);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  ui_progress_set(bar, caught, total);
}

}  // namespace

lv_obj_t *menu_screen_create() {
  lv_obj_t *scr = ui_screen();
  header(scr);
  caught_section(scr);

  // Tile grande: Lista.
  // Ajustes › Abrir em: o tile grande abre a Lista ou a Grade.
  lv_obj_t *big = tile(scr, kGridX, kGridY, kBigW, kGridH, true,
                       settings().open_grid ? Screen::Grid : Screen::List);
  lv_obj_t *icon = ui_icon(big, &icon_list_26, theme::ON_ACCENT);
  lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);
  char buf[24];
  snprintf(buf, sizeof(buf), tr(STR_MENU_SPECIES_FMT), species_repo_count());
  lv_obj_t *sub = ui_label(big, &font_12_600, theme::ON_ACCENT, buf);
  lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t *title = ui_label(big, &font_20_700, theme::ON_ACCENT, tr(STR_TITLE_LIST));
  lv_obj_align_to(title, sub, LV_ALIGN_OUT_TOP_LEFT, 0, -2);

  small_tile(scr, 0, 0, &icon_search_20, STR_TITLE_SEARCH, Screen::Search);
  small_tile(scr, 1, 0, &icon_tag_20, STR_TITLE_TYPES, Screen::Types);
  small_tile(scr, 0, 1, &icon_bars_20, STR_TITLE_PROGRESS, Screen::Progress);
  small_tile(scr, 1, 1, &icon_sliders_20, STR_TITLE_SETTINGS, Screen::Settings);

  return scr;
}
