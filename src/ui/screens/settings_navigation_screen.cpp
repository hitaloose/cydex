// 05f Ajustes › Navegação (canvas: AjustesNavegacao.dc.html).
//   Catálogo em: Lista | Grade (o tile Lista do Menu abre um ou outro)
//   Ao ligar: Menu | Vitrine (o Boot vai pra um ou outro)
#include "data/Settings.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/settings_page.h"
#include "ui/settings_widgets.h"
#include "ui/widgets.h"

namespace {

const StrId kViews[2] = {STR_TITLE_LIST, STR_TITLE_GRID};
const StrId kBoot[2] = {STR_SET_BOOT_MENU, STR_TITLE_SHOWCASE};

uint8_t s_page;
lv_obj_t *s_views;
lv_obj_t *s_boot;

void on_back(lv_event_t *) { nav_go(Screen::Settings); }

void on_view(lv_event_t *e) {
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  settings().open_grid = i == 1;
  catalog_state().view = i == 1 ? Screen::Grid : Screen::List;
  set_segmented_select(s_views, i);
  settings_save();
}

void on_boot(lv_event_t *e) {
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  settings().showcase_on_boot = i == 1;
  set_segmented_select(s_boot, i);
  settings_save();
}

void row_views(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_row(body, y, h);
  set_label(row, STR_SET_OPEN_IN);
  s_views = set_segmented(row, kViews, 2, settings().open_grid ? 1 : 0, on_view);
}

void row_boot(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_row(body, y, h);
  set_label(row, STR_SC_ON_BOOT);
  s_boot = set_segmented(row, kBoot, 2, settings().showcase_on_boot ? 1 : 0, on_boot);
}

const SettingsRow kRows[] = {
    {SETTINGS_ROW_H, row_views},
    {SETTINGS_ROW_H, row_boot},
};

}  // namespace

lv_obj_t *settings_navigation_screen_create() {
  lv_obj_t *scr = ui_screen();
  ui_header(scr, tr(STR_SET_NAVIGATION), on_back);
  settings_page(scr, kRows, sizeof(kRows) / sizeof(kRows[0]), &s_page);
  return scr;
}
