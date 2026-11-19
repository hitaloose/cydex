#include "catalog.h"

#include "data/SpeciesRepo.h"
#include "data/StatusStore.h"
#include "dex/Dex.h"
#include "i18n/i18n.h"
#include "ui/icons/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <cstdio>

static CatalogState s_state;
static uint16_t s_list[DEX_COUNT];  // 2 KB, estático
static uint16_t s_len = 0;
static bool s_dirty = true;

CatalogState &catalog_state() { return s_state; }

const uint16_t *catalog_list(uint16_t *len) {
  if (s_dirty) {
    s_len = catalog_build(s_state.query, species_repo_count(), status_store_get(),
                          species_repo_types, s_list, species_repo_name);
    s_dirty = false;
  }
  *len = s_len;
  return s_list;
}

void catalog_invalidate() { s_dirty = true; }

static void on_clear_refinements(lv_event_t *) {
  s_state.query.clear_refinements();
  s_state.anchor = 1;
  catalog_invalidate();
  nav_reload();
}

lv_obj_t *catalog_header(lv_obj_t *scr, lv_event_cb_t on_back) {
  const CatalogQuery &q = s_state.query;
  if (!q.refined()) return ui_header(scr, tr(STR_CATALOG_NATIONAL), on_back);

  lv_obj_t *h = ui_header(scr, nullptr, on_back);
  lv_obj_t *chip = ui_box(h);
  lv_obj_set_size(chip, LV_SIZE_CONTENT, 24);
  lv_obj_set_style_max_width(chip, 156, 0);  // sobra com voltar + 3 botões
  lv_obj_set_style_radius(chip, 12, 0);
  lv_obj_set_style_bg_color(chip, c(theme::SURFACE_2), 0);
  lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(chip, c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_set_style_pad_left(chip, 8, 0);
  lv_obj_set_style_pad_right(chip, 3, 0);
  lv_obj_set_style_pad_column(chip, 5, 0);
  lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_ext_click_area(chip, 4);  // 24 + 2x4 = alvo de 32 px
  ui_on_click(chip, on_clear_refinements);

  if (q.has_text()) {
    ui_icon(chip, &icon_search_12, theme::MUTED);
    char text[SEARCH_MAX + 8];
    snprintf(text, sizeof(text), tr(STR_SEARCH_CHIP_FMT), q.text);
    lv_obj_t *l = ui_label(chip, &font_12_700, theme::TEXT, text);
    lv_obj_set_style_max_width(l, 110, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  } else {
    const SpeciesType chosen[2] = {q.type_a, q.type_b};
    for (SpeciesType t : chosen) {
      if (t == SpeciesType::None) continue;
      lv_obj_t *dot = ui_box(chip);
      lv_obj_set_size(dot, 8, 8);
      lv_obj_set_style_radius(dot, 4, 0);
      lv_obj_set_style_bg_color(dot, c(species_type_color(t)), 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      ui_label(chip, &font_12_700, theme::TEXT, species_type_name(t));
    }
  }
  lv_obj_t *x = ui_box(chip);
  lv_obj_set_size(x, 18, 18);
  lv_obj_set_style_radius(x, 9, 0);
  lv_obj_set_style_bg_color(x, c(theme::LINE), 0);
  lv_obj_set_style_bg_opa(x, LV_OPA_COVER, 0);
  lv_obj_center(ui_icon(x, &icon_close_10, theme::MUTED));
  return h;
}
