// 05c Ajustes › Sistema (canvas: AjustesSistema.dc.html).
//   Wi-Fi › (rede atual, abre o 05d)
//   Idioma › (abre a Escolha; troca na hora e a tela volta já no idioma novo)
//   Cartão SD "1025 registros · 4 saves" + Recarregar (52 px)
#include "data/Settings.h"
#include "data/SpeciesRepo.h"
#include "data/StatusStore.h"
#include "i18n/i18n.h"
#include "net/wifi_mgr.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/settings_page.h"
#include "ui/settings_widgets.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kSdH = 52;

uint8_t s_page;

void on_back(lv_event_t *) { nav_go(Screen::Settings); }
void on_wifi(lv_event_t *) { nav_go(Screen::SettingsWifi); }

void on_reload(lv_event_t *) {
  boot_request_rescan();
  nav_go(Screen::Boot);
}

const char *wifi_summary() {
  switch (wifi_state()) {
    case WifiState::Off: return tr(STR_WIFI_OFF);
    case WifiState::Connected: return wifi_ssid();
    case WifiState::Connecting: return tr(STR_WIFI_CONNECTING);
    default: return tr(STR_WIFI_DISCONNECTED);
  }
}

// Escolha (05g) do idioma: nome no próprio idioma, código à direita.
const char *lang_label(uint8_t i) { return i18n_lang_name((LangId)i); }
const char *lang_hint(uint8_t i) { return i18n_lang_code((LangId)i); }
void lang_pick(uint8_t i) {
  if ((LangId)i != i18n_lang()) settings_set_lang((LangId)i);
}

void on_lang(lv_event_t *) {
  ChoiceSpec spec;
  spec.title = STR_SYS_LANGUAGE;
  spec.count = LANG_COUNT;
  spec.selected = (uint8_t)i18n_lang();
  spec.label = lang_label;
  spec.hint = lang_hint;
  spec.pick = lang_pick;
  spec.back = Screen::SettingsSystem;
  settings_choice_open(spec);
}

void row_wifi(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *row = set_link_row(body, y, h, &icon_wifi_16, STR_SET_WIFI, wifi_summary(), on_wifi, true);
  lv_obj_t *l = lv_obj_get_child(row, 3);  // resumo: nome de rede comprido não empurra a seta
  lv_obj_set_style_max_width(l, 190, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
}

void row_lang(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  set_link_row(body, y, h, &icon_globe_16, STR_SYS_LANGUAGE, i18n_lang_name(i18n_lang()), on_lang,
               true);
}

void row_sd(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t) {
  lv_obj_t *sd = set_row(body, y, h);
  ui_icon(sd, &icon_sd_16, theme::ACCENT);
  lv_obj_t *col = ui_box(sd);
  lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(col, 1, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  ui_label(col, &font_14_600, theme::TEXT, tr(STR_SYS_SD));
  lv_obj_t *info = ui_label(col, &font_12_500, theme::MUTED, "");
  lv_label_set_text_fmt(info, tr(STR_SYS_SD_INFO_FMT), species_repo_count(),
                        status_store_last_report().saves_found);
  ui_header_spacer(sd);

  lv_obj_t *btn = ui_box(sd);
  lv_obj_set_size(btn, LV_SIZE_CONTENT, 32);
  lv_obj_set_style_pad_hor(btn, 10, 0);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, c(theme::ACCENT), 0);
  lv_obj_set_style_bg_color(btn, c(theme::ACCENT_SOFT), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_t *l = ui_label(btn, &font_12_700, theme::ACCENT, tr(STR_SYS_RELOAD));
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  ui_on_click(btn, on_reload);
}

const SettingsRow kRows[] = {
    {SETTINGS_ROW_H, row_wifi},
    {SETTINGS_ROW_H, row_lang},
    {kSdH, row_sd},
};

}  // namespace

lv_obj_t *settings_system_screen_create() {
  lv_obj_t *scr = ui_screen();
  ui_header(scr, tr(STR_SET_SYSTEM), on_back);
  settings_page(scr, kRows, sizeof(kRows) / sizeof(kRows[0]), &s_page);
  return scr;
}
