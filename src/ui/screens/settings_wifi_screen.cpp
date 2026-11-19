// 05d Ajustes › Sistema › Wi-Fi (canvas: AjustesWiFi.dc.html; nota "Wi-Fi: regras").
//   cabeçalho 32: voltar, "Wi-Fi", ↻ (buscar de novo), ⓘ (Conexão, 05d')
//   linha 34: "Wi-Fi", estado ("Casa · 192.168.0.12", "Buscando…") e interruptor
//   4 redes por página (34 px): barras de sinal, SSID, cadeado / "aberta" /
//   ✓ + Esquecer na rede atual (que fica no topo)
//   rodapé de paginação (o meio só mostra a contagem)
// Um timer de 250 ms compara wifi_version() e só redesenha quando algo mudou
// (fim da busca, conectou, caiu). A busca começa ao abrir a tela e no ↻.
#include <cstdio>
#include <cstring>

#include "data/Settings.h"
#include "dex/DexStatus.h"
#include "i18n/i18n.h"
#include "net/wifi_mgr.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/pager_bar.h"
#include "ui/settings_widgets.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kStatusY = theme::HEADER_H;
constexpr lv_coord_t kListY = kStatusY + theme::ROW_H;
constexpr lv_coord_t kListH = 240 - kListY - theme::PAGER_H;
constexpr uint8_t kPerPage = 4;

lv_obj_t *s_status;
lv_obj_t *s_refresh;
lv_obj_t *s_list;
lv_timer_t *s_timer;
PagerBar s_pager;
uint32_t s_seen_version;

void refresh();

void on_back(lv_event_t *) { nav_go(Screen::SettingsSystem); }
void on_info(lv_event_t *) { nav_go(Screen::WifiInfo); }

void on_refresh(lv_event_t *) {
  wifi_scan_start();
  refresh();
}

void on_toggle(lv_event_t *e) {
  bool on = set_switch_get(lv_event_get_current_target(e));
  wifi_set_enabled(on);
  if (on) wifi_scan_start();
  s_pager.pager.page = 0;
  refresh();
}

void on_forget(lv_event_t *) {
  wifi_forget();
  refresh();
}

void on_net(lv_event_t *e) {
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  const WifiList &l = wifi_scan_results();
  if (i >= l.count) return;
  const WifiNet &n = l.items[i];
  if (n.secure) {
    wifi_set_pending_ssid(n.ssid);
    nav_go(Screen::WifiPassword);
  } else {
    wifi_connect(n.ssid, "");
    refresh();
  }
}

void on_page(bool) { refresh(); }

void on_timer(lv_timer_t *) {
  if (wifi_version() != s_seen_version) refresh();
}

void on_delete(lv_event_t *) {
  if (s_timer) lv_timer_del(s_timer);
  s_timer = nullptr;
}

// 4 barrinhas (canvas: x 1/5/9/13, alturas 4/7/10/13, largura 3).
void signal_bars(lv_obj_t *row, uint8_t level) {
  lv_obj_t *box = ui_box(row);
  lv_obj_set_size(box, 16, 16);
  static const uint8_t kH[4] = {4, 7, 10, 13};
  for (uint8_t i = 0; i < 4; i++) {
    lv_obj_t *b = ui_box(box);
    lv_obj_set_pos(b, 1 + i * 4, 15 - kH[i]);
    lv_obj_set_size(b, 3, kH[i]);
    lv_obj_set_style_radius(b, 1, 0);
    lv_obj_set_style_bg_color(b, c(i < level ? theme::TEXT : theme::FAINT_2), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  }
}

const char *fail_text() {
  switch (wifi_fail_reason()) {
    case WifiFail::WrongPassword: return tr(STR_WIFI_WRONG_PASSWORD);
    case WifiFail::NotFound: return tr(STR_WIFI_NOT_FOUND);
    default: return tr(STR_WIFI_FAILED);
  }
}

void centered_message(const lv_img_dsc_t *icon, const char *title, const char *body) {
  lv_obj_t *col = ui_box(s_list);
  lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_hor(col, 28, 0);
  lv_obj_set_style_pad_row(col, 6, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (icon) ui_icon(col, icon, theme::FAINT);
  ui_label(col, &font_14_600, body ? theme::TEXT : theme::MUTED, title);
  if (body) {
    lv_obj_t *l = ui_label(col, &font_12_500, theme::MUTED, body);
    lv_obj_set_width(l, 320 - 56);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
}

void refresh() {
  s_seen_version = wifi_version();
  WifiState st = wifi_state();
  bool on = st != WifiState::Off;
  bool scanning = wifi_scanning();

  // Linha de estado.
  char text[64];
  uint32_t color = theme::MUTED;
  if (!on) snprintf(text, sizeof(text), "%s", tr(STR_WIFI_OFF));
  else if (scanning) snprintf(text, sizeof(text), "%s", tr(STR_WIFI_SCANNING));
  else if (st == WifiState::Connecting) snprintf(text, sizeof(text), "%s", tr(STR_WIFI_CONNECTING));
  else if (st == WifiState::Connected) {
    snprintf(text, sizeof(text), "%s · %s", wifi_ssid(), wifi_ip());
    color = theme::TEXT;
  } else if (st == WifiState::Failed) {
    snprintf(text, sizeof(text), "%s", fail_text());
    color = theme::ACCENT;
  } else snprintf(text, sizeof(text), "%s", tr(STR_WIFI_DISCONNECTED));
  lv_label_set_text(s_status, text);
  lv_obj_set_style_text_color(s_status, c(color), 0);

  bool can_scan = on && !scanning && st != WifiState::Connecting;
  lv_obj_set_style_img_recolor(lv_obj_get_child(s_refresh, 0), c(can_scan ? theme::TEXT : theme::FAINT_2), 0);
  if (can_scan) lv_obj_add_flag(s_refresh, LV_OBJ_FLAG_CLICKABLE);
  else lv_obj_clear_flag(s_refresh, LV_OBJ_FLAG_CLICKABLE);

  // Lista.
  lv_obj_clean(s_list);
  const WifiList &l = wifi_scan_results();
  uint16_t total = on && !scanning ? l.count : 0;
  uint16_t page = s_pager.pager.page;
  s_pager.pager.reset(total, kPerPage);
  s_pager.pager.go_to_page(page);

  if (!on) {
    centered_message(&icon_wifi_off_28, tr(STR_WIFI_OFF_TITLE), tr(STR_WIFI_OFF_BODY));
  } else if (scanning) {
    centered_message(nullptr, tr(STR_WIFI_SCANNING_LONG), nullptr);
  } else if (total == 0) {
    centered_message(nullptr, tr(STR_WIFI_NONE), nullptr);
  } else {
    const char *cur = wifi_ssid();
    bool connected = st == WifiState::Connected;
    bool connecting = st == WifiState::Connecting;
    uint16_t first = s_pager.pager.first_index();
    for (uint16_t k = 0; k < s_pager.pager.items_on_page(); k++) {
      uint8_t i = (uint8_t)(first + k);
      const WifiNet &n = l.items[i];
      bool is_cur = cur[0] && strcmp(n.ssid, cur) == 0 && (connected || connecting);
      lv_obj_t *row = set_row(s_list, k * theme::ROW_H, theme::ROW_H);
      lv_obj_set_style_pad_column(row, 8, 0);
      signal_bars(row, wifi_bars(n.rssi));
      lv_obj_t *name = ui_label(row, is_cur ? &font_14_700 : &font_14_600,
                                is_cur && connected ? theme::ACCENT : theme::TEXT, n.ssid);
      lv_obj_set_width(name, 1);
      lv_obj_set_flex_grow(name, 1);
      lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
      if (is_cur && connected) {
        lv_obj_set_style_pad_right(row, 6, 0);
        ui_status_mark_set(ui_status_mark(row, 14), SpeciesState::Caught);
        lv_obj_t *btn = ui_box(row);
        lv_obj_set_size(btn, LV_SIZE_CONTENT, 28);
        lv_obj_set_style_pad_hor(btn, 10, 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, c(theme::LINE), 0);
        lv_obj_set_style_bg_color(btn, c(theme::SURFACE), 0);
        lv_obj_set_style_bg_color(btn, c(theme::SURFACE_2), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_ext_click_area(btn, 3);
        lv_obj_t *t = ui_label(btn, &font_12_700, theme::TEXT, tr(STR_WIFI_FORGET));
        lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);
        ui_on_click(btn, on_forget);
        continue;  // a rede atual não é clicável (já está conectada)
      }
      if (is_cur) ui_label(row, &font_12_500, theme::MUTED, tr(STR_WIFI_CONNECTING));
      else if (n.secure) ui_icon(row, &icon_lock_12, theme::MUTED);
      else ui_label(row, &font_12_500, theme::MUTED, tr(STR_WIFI_OPEN));
      if (!is_cur) {
        lv_obj_set_style_bg_color(row, c(theme::SURFACE), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
        ui_on_click(row, on_net, (void *)(uintptr_t)i);
      }
    }
  }

  // Rodapé: contagem no meio.
  char count[24];
  if (!on) snprintf(count, sizeof(count), "—");
  else if (scanning) snprintf(count, sizeof(count), "…");
  else if (total == 1) snprintf(count, sizeof(count), "%s", tr(STR_WIFI_NETWORKS_ONE));
  else snprintf(count, sizeof(count), tr(STR_WIFI_NETWORKS_FMT), total);
  pager_bar_refresh(&s_pager, count);
  if (!on || scanning) lv_label_set_text(s_pager.page, "");
}

}  // namespace

lv_obj_t *settings_wifi_screen_create() {
  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);

  lv_obj_t *h = ui_header(scr, tr(STR_SET_WIFI), on_back);
  ui_header_spacer(h);
  s_refresh = ui_header_button(h, &icon_refresh_18, on_refresh);
  ui_header_button(h, &icon_info_18, on_info);

  lv_obj_t *row = set_row(scr, kStatusY, theme::ROW_H);
  ui_label(row, &font_14_600, theme::TEXT, tr(STR_SET_WIFI));
  s_status = ui_label(row, &font_12_500, theme::MUTED, "");
  lv_obj_set_width(s_status, 1);
  lv_obj_set_flex_grow(s_status, 1);
  lv_label_set_long_mode(s_status, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_RIGHT, 0);
  set_switch(row, wifi_state() != WifiState::Off, on_toggle);

  s_list = ui_box(scr);
  lv_obj_set_pos(s_list, 0, kListY);
  lv_obj_set_size(s_list, 320, kListH);

  s_pager.on_page = on_page;
  s_pager.on_center = nullptr;
  s_pager.pager.page = 0;
  pager_bar_create(&s_pager, scr);

  if (wifi_state() != WifiState::Off) wifi_scan_start();
  refresh();
  s_timer = lv_timer_create(on_timer, 250, nullptr);
  return scr;
}
