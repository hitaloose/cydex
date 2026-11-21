// 05 Ajustes (canvas: Ajustes.dc.html; regras na nota "Ajustes: regras").
//   raiz da árvore: uma linha "›" por grupo, com o resumo do que tem dentro
//   Tela e som (05a) · Navegação (05f) · Modo vitrine (05b) · Sistema (05c)
#include <cstdio>

#include "data/Settings.h"
#include "i18n/i18n.h"
#include "net/wifi_mgr.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/power.h"
#include "ui/settings_page.h"
#include "ui/settings_widgets.h"
#include "ui/widgets.h"

// "1 min" / "30 s": também usado no 05b.
void format_interval(uint16_t seconds, char *out, size_t size) {
  if (seconds < 60) snprintf(out, size, tr(STR_SET_SECONDS_FMT), seconds);
  else snprintf(out, size, tr(STR_SET_MINUTES_FMT), seconds / 60);
}

// "2 min" / "nunca": 05 e 05a.
void format_screen_off(uint16_t minutes, char *out, size_t size) {
  if (minutes == 0) snprintf(out, size, "%s", tr(STR_SET_NEVER));
  else snprintf(out, size, tr(STR_SET_MINUTES_FMT), minutes);
}

namespace {

uint8_t s_page;

void on_back(lv_event_t *) { nav_go(Screen::Menu); }
void on_display(lv_event_t *) { nav_go(Screen::SettingsDisplay); }
void on_navigation(lv_event_t *) { nav_go(Screen::SettingsNavigation); }
void on_showcase(lv_event_t *) { nav_go(Screen::SettingsShowcase); }
void on_system(lv_event_t *) { nav_go(Screen::SettingsSystem); }

void row_display(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  const AppSettings &st = settings();
  char off[16], summary[32];
  format_screen_off(st.screen_off_min, off, sizeof(off));
  // O brilho que vale agora (de noite, o noturno).
  snprintf(summary, sizeof(summary), "%u%% · %s",
           power_is_night() ? st.brightness_night : st.brightness, off);
  set_link_row(body, y, h, &icon_sun_16, STR_SET_DISPLAY, summary, on_display, true);
}

void row_navigation(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  const AppSettings &st = settings();
  char summary[48];
  snprintf(summary, sizeof(summary), "%s · %s", tr(st.open_grid ? STR_TITLE_GRID : STR_TITLE_LIST),
           tr(st.showcase_on_boot ? STR_TITLE_SHOWCASE : STR_SET_BOOT_MENU));
  set_link_row(body, y, h, &icon_compass_16, STR_SET_NAVIGATION, summary, on_navigation, true);
}

void row_showcase(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  const AppSettings &st = settings();
  char interval[16], summary[48];
  format_interval(st.showcase_interval_s, interval, sizeof(interval));
  snprintf(summary, sizeof(summary), "%s · %s", interval,
           tr(st.showcase_random ? STR_SC_ORDER_RAND_SHORT : STR_SC_ORDER_SEQ_SHORT));
  set_link_row(body, y, h, &icon_monitor_16, STR_TITLE_SETTINGS_SHOWCASE, summary, on_showcase,
               true);
}

void row_system(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  char summary[64];
  const char *lang = i18n_lang_name(i18n_lang());
  if (wifi_state() == WifiState::Connected) snprintf(summary, sizeof(summary), "%s · %s", lang, wifi_ssid());
  else snprintf(summary, sizeof(summary), "%s", lang);
  lv_obj_t *row = set_link_row(body, y, h, &icon_chip_16, STR_SET_SYSTEM, summary, on_system, true);
  // Nome de rede comprido não empurra a seta pra fora.
  lv_obj_t *l = lv_obj_get_child(row, 3);
  lv_obj_set_style_max_width(l, 170, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
}

const SettingsRow kRows[] = {
    {SETTINGS_ROW_H, row_display},
    {SETTINGS_ROW_H, row_navigation},
    {SETTINGS_ROW_H, row_showcase},
    {SETTINGS_ROW_H, row_system},
};

}  // namespace

lv_obj_t *settings_screen_create() {
  lv_obj_t *scr = ui_screen();
  set_header_version(ui_header(scr, tr(STR_TITLE_SETTINGS), on_back));
  settings_page(scr, kRows, sizeof(kRows) / sizeof(kRows[0]), &s_page);
  return scr;
}
