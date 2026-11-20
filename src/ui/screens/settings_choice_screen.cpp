// 05g Ajustes › Escolha (canvas: AjustesEscolha.dc.html).
//   uma linha de 34 px por opção: nome, dica muted à direita, ✓ na atual
//   tocar grava (spec.pick) e volta pra spec.back; com 6+ opções, pagina
// Genérica: quem abre (Mostrar no 05b, Idioma no 05c) preenche um ChoiceSpec.
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/settings_page.h"
#include "ui/settings_widgets.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr uint8_t kMaxOptions = 32;

ChoiceSpec s_spec;
SettingsRow s_rows[kMaxOptions];
uint8_t s_page;

void on_back(lv_event_t *) { nav_go(s_spec.back); }

void on_pick(lv_event_t *e) {
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (s_spec.pick) s_spec.pick(i);
  nav_go(s_spec.back);  // remonta quem abriu (no idioma novo, se for o caso)
}

void row_option(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t i) {
  lv_obj_t *row = set_row(body, y, h);
  lv_obj_set_style_bg_color(row, c(theme::SURFACE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
  bool on = i == s_spec.selected;
  ui_label(row, &font_14_600, on ? theme::ACCENT : theme::TEXT, s_spec.label(i));
  ui_header_spacer(row);
  if (s_spec.hint) ui_label(row, &font_12_500, theme::MUTED, s_spec.hint(i));
  lv_obj_t *check = ui_icon(row, &icon_check_14, theme::ACCENT);
  if (!on) lv_obj_set_style_img_opa(check, LV_OPA_TRANSP, 0);  // guarda o lugar: dicas alinhadas
  ui_on_click(row, on_pick, (void *)(uintptr_t)i);
}

}  // namespace

void settings_choice_open(const ChoiceSpec &spec) {
  s_spec = spec;
  if (s_spec.count > kMaxOptions) s_spec.count = kMaxOptions;
  for (uint8_t i = 0; i < s_spec.count; i++) s_rows[i] = {SETTINGS_ROW_H, row_option};
  s_page = settings_page_of(s_rows, s_spec.count, s_spec.selected);  // abre na opção atual
  nav_go(Screen::SettingsChoice);
}

lv_obj_t *settings_choice_screen_create() {
  lv_obj_t *scr = ui_screen();
  ui_header(scr, tr(s_spec.title), on_back);
  settings_page(scr, s_rows, s_spec.count, &s_page);
  return scr;
}
