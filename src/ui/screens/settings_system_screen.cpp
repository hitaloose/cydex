// 05c Ajustes › Sistema (canvas: AjustesSistema.dc.html).
//   "Idioma" + uma linha de 34 px por idioma (troca na hora, nav_reload)
//   embaixo (52 px): Cartão SD "1025 registros · 4 saves" + Recarregar
#include "data/Settings.h"
#include "data/SpeciesRepo.h"
#include "data/StatusStore.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/settings_widgets.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kCaptionH = 24, kRowH = 34, kSdH = 52;

void on_back(lv_event_t *) { nav_go(Screen::Settings); }

void on_lang(lv_event_t *e) {
  LangId lang = (LangId)(uintptr_t)lv_event_get_user_data(e);
  if (lang == i18n_lang()) return;
  settings_set_lang(lang);
  nav_reload();  // remonta esta tela já no idioma novo
}

void on_reload(lv_event_t *) {
  boot_request_rescan();
  nav_go(Screen::Boot);
}

void radio(lv_obj_t *row, bool on) {
  lv_obj_t *ring = ui_box(row);
  lv_obj_set_size(ring, 18, 18);
  lv_obj_set_style_radius(ring, 9, 0);
  lv_obj_set_style_border_width(ring, 2, 0);
  lv_obj_set_style_border_color(ring, c(on ? theme::ACCENT : theme::FAINT), 0);
  if (!on) return;
  lv_obj_t *dot = ui_box(ring);
  lv_obj_set_size(dot, 8, 8);
  lv_obj_set_style_radius(dot, 4, 0);
  lv_obj_set_style_bg_color(dot, c(theme::ACCENT), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_center(dot);
}

}  // namespace

lv_obj_t *settings_system_screen_create() {
  lv_obj_t *scr = ui_screen();
  set_header_version(ui_header(scr, tr(STR_SET_SYSTEM), on_back));

  lv_coord_t y = theme::HEADER_H;
  lv_obj_t *cap = ui_label(scr, &font_12_600, theme::MUTED, tr(STR_SYS_LANGUAGE));
  lv_obj_set_pos(cap, 10, y + kCaptionH - 16);
  y += kCaptionH;

  for (int i = 0; i < LANG_COUNT; i++) {
    LangId lang = (LangId)i;
    lv_obj_t *row = set_row(scr, y, kRowH);
    lv_obj_set_style_bg_color(row, c(theme::SURFACE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    radio(row, lang == i18n_lang());
    ui_label(row, &font_14_600, theme::TEXT, i18n_lang_name(lang));
    ui_header_spacer(row);
    ui_label(row, &font_12_500, theme::MUTED, i18n_lang_code(lang));
    ui_on_click(row, on_lang, (void *)(uintptr_t)lang);
    y += kRowH;
  }

  // Cartão SD, colado embaixo.
  lv_obj_t *sd = set_row(scr, 240 - kSdH, kSdH, false);
  lv_obj_set_style_border_side(sd, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(sd, 1, 0);
  lv_obj_set_style_border_color(sd, c(theme::ROW_DIVIDER), 0);
  ui_icon(sd, &icon_sd_16, theme::MUTED);
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
  return scr;
}
