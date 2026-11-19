// 05d' Wi-Fi › Conexão (canvas: AjustesWiFiInfo.dc.html).
//   3 linhas de 34 px: Endereço (cydex.local), IP, Chave do hook
//   embaixo: como configurar o hook do Claude Code no computador
#include <cstdio>

#include "data/Settings.h"
#include "i18n/i18n.h"
#include "net/wifi_mgr.h"
#include "screens.h"
#include "ui/settings_widgets.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

void on_back(lv_event_t *) { nav_go(Screen::SettingsWifi); }

lv_obj_t *info_row(lv_obj_t *scr, lv_coord_t y, StrId label) {
  lv_obj_t *row = set_row(scr, y, theme::ROW_H);
  set_label(row, label);
  return row;
}

}  // namespace

lv_obj_t *wifi_info_screen_create() {
  lv_obj_t *scr = ui_screen();
  ui_header(scr, tr(STR_WIFI_INFO_TITLE), on_back);

  lv_coord_t y = theme::HEADER_H;
  ui_label(info_row(scr, y, STR_WIFI_INFO_ADDRESS), &font_14_700, theme::TEXT, "cydex.local");
  y += theme::ROW_H;
  const char *ip = wifi_ip();
  ui_label(info_row(scr, y, STR_WIFI_INFO_IP), &font_14_600, theme::MUTED, ip[0] ? ip : "—");
  y += theme::ROW_H;

  const char *key = settings().hook_key;
  lv_obj_t *chip = ui_box(info_row(scr, y, STR_WIFI_INFO_KEY));
  lv_obj_set_size(chip, LV_SIZE_CONTENT, 22);
  lv_obj_set_style_pad_hor(chip, 8, 0);
  lv_obj_set_style_radius(chip, 6, 0);
  lv_obj_set_style_bg_color(chip, c(theme::ACCENT_SOFT), 0);
  lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
  lv_obj_t *k = ui_label(chip, &font_14_700, theme::ACCENT, key);
  lv_obj_set_style_text_letter_space(k, 1, 0);
  lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);
  y += theme::ROW_H;

  lv_obj_t *col = ui_box(scr);
  lv_obj_set_pos(col, 0, y);
  lv_obj_set_size(col, 320, 240 - y);
  lv_obj_set_style_pad_all(col, 10, 0);
  lv_obj_set_style_pad_row(col, 6, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  ui_label(col, &font_12_700, theme::MUTED, tr(STR_WIFI_INFO_PC));
  char help[160];
  snprintf(help, sizeof(help), tr(STR_WIFI_INFO_HELP_FMT), key);
  lv_obj_t *h = ui_label(col, &font_12_500, theme::TEXT, help);
  lv_obj_set_width(h, 300);
  lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
  lv_obj_t *ex = ui_label(col, &font_12_500, theme::MUTED, tr(STR_WIFI_INFO_EXAMPLE));
  lv_obj_set_width(ex, 300);
  lv_label_set_long_mode(ex, LV_LABEL_LONG_WRAP);
  return scr;
}
