// 05e Wi-Fi › Senha (canvas: WiFiSenha.dc.html; nota "Wi-Fi: regras").
//   cabeçalho 36: voltar, campo (cadeado, senha mascarada, olho)
//   faixa 28: rede escolhida + estado (Conectando… / Senha incorreta / Conectado)
//   teclado de 40 px (ui/keyboard, layout Text) com "Conectar"
// Conectar chama wifi_connect; um timer de 250 ms acompanha o resultado. Deu
// certo: o botão vira "Pronto" (volta pro Wi-Fi). Senha errada: o texto fica.
#include <cstdio>
#include <cstring>

#include "i18n/i18n.h"
#include "net/wifi_mgr.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/keyboard.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kStripY = theme::HEADER_SEARCH_H;
constexpr lv_coord_t kStripH = 28;
constexpr lv_coord_t kKbY = kStripY + kStripH;
constexpr size_t kPassMax = 63;   // WPA2: até 63 caracteres
constexpr size_t kVisible = 24;   // o campo mostra os últimos 24

char s_pw[kPassMax + 1];
bool s_show;
bool s_waiting;  // pedimos pra conectar e esperamos o resultado
bool s_done;
lv_obj_t *s_field_box;
lv_obj_t *s_field;
lv_obj_t *s_eye;
lv_obj_t *s_status;
lv_timer_t *s_timer;
uint32_t s_seen_version;

void on_back(lv_event_t *) { nav_go(Screen::SettingsWifi); }

void paint_field() {
  size_t len = strlen(s_pw);
  if (len == 0) {
    lv_label_set_text(s_field, tr(STR_WIFI_PASSWORD));
    lv_obj_set_style_text_color(s_field, c(theme::MUTED), 0);
    return;
  }
  size_t from = len > kVisible ? len - kVisible : 0;
  char out[kVisible * 3 + 1];
  size_t o = 0;
  for (size_t i = from; i < len; i++) {
    if (s_show) {
      out[o++] = s_pw[i];
    } else {
      memcpy(out + o, "\xE2\x80\xA2", 3);  // •
      o += 3;
    }
  }
  out[o] = '\0';
  lv_label_set_text(s_field, out);
  lv_obj_set_style_text_color(s_field, c(theme::TEXT), 0);
}

void set_status(const char *text, uint32_t color, bool error) {
  lv_label_set_text(s_status, text);
  lv_obj_set_style_text_color(s_status, c(color), 0);
  lv_obj_set_style_border_color(s_field_box, c(error ? theme::ACCENT : theme::LINE), 0);
}

void edited() {
  if (s_waiting) return;
  s_done = false;
  set_status("", theme::MUTED, false);
  keyboard_set_action(STR_WIFI_CONNECT, s_pw[0] != '\0');
  paint_field();
}

void on_char(char ch) {
  size_t len = strlen(s_pw);
  if (s_waiting || len >= kPassMax) return;
  s_pw[len] = ch;
  s_pw[len + 1] = '\0';
  edited();
}

void on_space() { on_char(' '); }

void on_backspace() {
  size_t len = strlen(s_pw);
  if (s_waiting || len == 0) return;
  s_pw[len - 1] = '\0';
  edited();
}

void on_action() {
  if (s_done) {
    nav_go(Screen::SettingsWifi);
    return;
  }
  if (s_waiting || !s_pw[0]) return;
  wifi_connect(wifi_pending_ssid(), s_pw);
  s_waiting = true;
  s_seen_version = wifi_version();
  set_status(tr(STR_WIFI_CONNECTING), theme::MUTED, false);
  keyboard_set_action(STR_WIFI_CONNECT, false);
}

void on_eye(lv_event_t *) {
  s_show = !s_show;
  lv_img_set_src(s_eye, s_show ? &icon_eye_off_16 : &icon_eye_16);
  paint_field();
}

void on_timer(lv_timer_t *) {
  if (!s_waiting || wifi_version() == s_seen_version) return;
  s_seen_version = wifi_version();
  if (strcmp(wifi_ssid(), wifi_pending_ssid()) != 0) return;
  switch (wifi_state()) {
    case WifiState::Connected:
      s_waiting = false;
      s_done = true;
      set_status(tr(STR_WIFI_CONNECTED), theme::TEXT, false);
      keyboard_set_action(STR_WIFI_DONE, true);
      break;
    case WifiState::Failed: {
      s_waiting = false;
      WifiFail f = wifi_fail_reason();
      set_status(tr(f == WifiFail::WrongPassword ? STR_WIFI_WRONG_PASSWORD
                    : f == WifiFail::NotFound    ? STR_WIFI_NOT_FOUND
                                                 : STR_WIFI_FAILED),
                 theme::ACCENT, true);
      keyboard_set_action(STR_WIFI_CONNECT, s_pw[0] != '\0');
      break;
    }
    default:
      break;
  }
}

void on_delete(lv_event_t *) {
  if (s_timer) lv_timer_del(s_timer);
  s_timer = nullptr;
  // A senha não fica na RAM depois de sair (a salva vai pros Ajustes).
  memset(s_pw, 0, sizeof(s_pw));
}

}  // namespace

lv_obj_t *wifi_password_screen_create() {
  s_pw[0] = '\0';
  s_show = false;
  s_waiting = false;
  s_done = false;

  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);

  // Cabeçalho de 36 px com o campo (igual à Busca).
  lv_obj_t *h = ui_box(scr);
  lv_obj_set_size(h, 320, theme::HEADER_SEARCH_H);
  lv_obj_set_style_bg_color(h, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(h, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(h, 1, 0);
  lv_obj_set_style_border_color(h, c(theme::LINE), 0);
  lv_obj_set_style_pad_right(h, 4, 0);
  lv_obj_set_style_pad_column(h, 2, 0);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *back = ui_box(h);
  lv_obj_set_size(back, 36, theme::HEADER_SEARCH_H - 1);
  lv_obj_center(ui_icon(back, &icon_back_20, theme::TEXT));
  ui_on_click(back, on_back);

  s_field_box = ui_box(h);
  lv_obj_set_height(s_field_box, 28);
  lv_obj_set_flex_grow(s_field_box, 1);
  lv_obj_set_style_radius(s_field_box, 6, 0);
  lv_obj_set_style_border_width(s_field_box, 1, 0);
  lv_obj_set_style_border_color(s_field_box, c(theme::LINE), 0);
  lv_obj_set_style_bg_color(s_field_box, c(theme::BG), 0);
  lv_obj_set_style_bg_opa(s_field_box, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left(s_field_box, 8, 0);
  lv_obj_set_style_pad_right(s_field_box, 2, 0);
  lv_obj_set_style_pad_column(s_field_box, 6, 0);
  lv_obj_set_flex_flow(s_field_box, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_field_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  ui_icon(s_field_box, &icon_lock_14, theme::MUTED);
  s_field = ui_label(s_field_box, &font_14_600, theme::TEXT, "");
  lv_obj_set_width(s_field, 1);
  lv_obj_set_flex_grow(s_field, 1);
  lv_label_set_long_mode(s_field, LV_LABEL_LONG_CLIP);
  lv_obj_t *eye = ui_box(s_field_box);
  lv_obj_set_size(eye, 28, 24);
  lv_obj_set_style_radius(eye, 12, 0);
  lv_obj_set_style_bg_color(eye, c(theme::SURFACE_2), 0);
  lv_obj_set_style_bg_color(eye, c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
  lv_obj_set_ext_click_area(eye, 6);
  s_eye = ui_icon(eye, &icon_eye_16, theme::TEXT);
  lv_obj_center(s_eye);
  ui_on_click(eye, on_eye);

  // Faixa: rede + estado.
  lv_obj_t *strip = ui_box(scr);
  lv_obj_set_pos(strip, 0, kStripY);
  lv_obj_set_size(strip, 320, kStripH);
  lv_obj_set_style_pad_hor(strip, 10, 0);
  lv_obj_set_style_pad_column(strip, 6, 0);
  lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_icon(strip, &icon_wifi_14, theme::MUTED);
  lv_obj_t *ssid = ui_label(strip, &font_12_600, theme::TEXT, wifi_pending_ssid());
  lv_obj_set_width(ssid, 1);
  lv_obj_set_flex_grow(ssid, 1);
  lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
  s_status = ui_label(strip, &font_12_700, theme::MUTED, "");

  lv_obj_t *kb = ui_box(scr);
  lv_obj_set_pos(kb, 0, kKbY);
  lv_obj_set_size(kb, 320, 240 - kKbY);
  keyboard_create(kb, KbLayout::Text, STR_WIFI_CONNECT, {on_char, on_space, on_backspace, on_action});
  keyboard_set_action(STR_WIFI_CONNECT, false);

  paint_field();
  s_timer = lv_timer_create(on_timer, 250, nullptr);
  return scr;
}
