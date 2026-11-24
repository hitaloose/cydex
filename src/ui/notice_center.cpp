#include "notice_center.h"

#include <Arduino.h>

#include <cstdio>

#include "data/Settings.h"
#include "hal/speaker.h"
#include "net/notice_server.h"
#include "ui/icons/icons.h"
#include "ui/nav.h"
#include "ui/power.h"
#include "ui/screens/screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "util/log.h"

// Só desenha o NoticeBoard (net/notice_server): quem põe, ordena e limpa é ele.
// Redesenha quando a versão do quadro muda ou quando entra/sai da Vitrine.

namespace {

constexpr lv_coord_t kBannerH = 56;

lv_obj_t *s_obj = nullptr;        // faixa ou balões (nullptr = nada desenhado)
bool s_in_showcase = false;       // o que está desenhado é dos balões
uint32_t s_drawn_version = 0;
bool s_drawn = false;

void on_obj_delete(lv_event_t *e) {
  // Os balões morrem junto com a tela da Vitrine.
  if (lv_event_get_target(e) == s_obj) {
    s_obj = nullptr;
    s_drawn = false;
  }
}

// Toque na faixa (índice 0) ou num balão (índice em user_data). O quadro muda
// de versão e a próxima volta redesenha (apagar aqui, dentro do evento, não).
void on_dismiss(lv_event_t *e) {
  notices().dismiss((uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

void clear_drawn() {
  if (s_obj) {
    lv_obj_t *o = s_obj;
    s_obj = nullptr;
    lv_obj_del(o);
  }
  s_drawn = false;
}

lv_obj_t *banner(const Notice &n, uint8_t more) {
  lv_obj_t *b = ui_box(lv_layer_top());
  lv_obj_set_pos(b, 0, 0);
  lv_obj_set_size(b, 320, kBannerH);
  lv_obj_set_style_bg_color(b, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, c(theme::LINE), 0);
  lv_obj_set_style_pad_right(b, 10, 0);
  lv_obj_set_style_pad_column(b, 10, 0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  uint32_t color = n.color;
  lv_obj_t *bar = ui_box(b);
  lv_obj_set_size(bar, 4, LV_PCT(100));
  lv_obj_set_style_bg_color(bar, c(color), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  ui_icon(b, notice_icon(n.icon, false), color);

  // Largura do texto: 320 - barra 4 - ícone 20 - 3 gaps de 10 - pad 10 - chip.
  lv_coord_t text_w = 320 - 4 - 20 - 30 - 10 - (more ? 40 : 0);
  lv_obj_t *col = ui_box(b);
  lv_obj_set_size(col, text_w, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(col, 1, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *t = ui_label(col, &font_14_700, theme::TEXT, n.title);
  lv_obj_set_width(t, text_w);
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
  // "subtitle · message"; sem um dos dois, só o outro; sem os dois, só o título.
  char sub[sizeof(n.subtitle) + sizeof(n.message) + 8];
  const char *sep = n.subtitle[0] && n.message[0] ? " · " : "";
  snprintf(sub, sizeof(sub), "%s%s%s", n.subtitle, sep, n.message);
  if (sub[0]) {
    lv_obj_t *s = ui_label(col, &font_12_500, theme::MUTED, sub);
    ui_label_clamp(s, text_w, 2);
  }

  if (more) {
    lv_obj_t *chip = ui_box(b);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 20);
    lv_obj_set_style_radius(chip, 10, 0);
    lv_obj_set_style_pad_hor(chip, 7, 0);
    lv_obj_set_style_bg_color(chip, c(theme::SURFACE_2), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_t *l = ui_label(chip, &font_12_700, theme::TEXT, "");
    lv_label_set_text_fmt(l, "+%u", more);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  }
  ui_on_click(b, on_dismiss, (void *)(uintptr_t)0);
  return b;
}

void redraw() {
  clear_drawn();
  const NoticeBoard &board = notices();
  s_drawn_version = board.version();
  s_in_showcase = nav_current() == Screen::Showcase;
  s_drawn = true;
  if (board.size() == 0) return;

  lv_obj_t *o = nullptr;
  if (s_in_showcase) {
    Notice items[NOTICE_BOARD_CAP];
    uint32_t revs[NOTICE_BOARD_CAP];
    uint8_t n = board.size();
    for (uint8_t i = 0; i < n; i++) {
      items[i] = board.at(i);
      revs[i] = board.rev(i);
    }
    o = showcase_show_notices(items, revs, n, on_dismiss);
  }
  if (!o) {
    s_in_showcase = false;
    o = banner(board.at(0), (uint8_t)(board.size() - 1));
  }
  s_obj = o;
  lv_obj_add_event_cb(o, on_obj_delete, LV_EVENT_DELETE, nullptr);
}

}  // namespace

const lv_img_dsc_t *notice_icon(NoticeIcon icon, bool small) {
  switch (icon) {
    case NoticeIcon::Check: return small ? &icon_check_circle_16 : &icon_check_circle_20;
    case NoticeIcon::Alert: return small ? &icon_alert_16 : &icon_alert_20;
    case NoticeIcon::Shield: return small ? &icon_shield_16 : &icon_shield_20;
    case NoticeIcon::Clock: return small ? &icon_clock_16 : &icon_clock_20;
    case NoticeIcon::Bell: return small ? &icon_bell_16 : &icon_bell_20;
    case NoticeIcon::Terminal: return small ? &icon_terminal_16 : &icon_terminal_20;
    default: return small ? &icon_info_16 : &icon_info_20;
  }
}

const lv_img_dsc_t *notice_icon_12(NoticeIcon icon) {
  switch (icon) {
    case NoticeIcon::Check: return &icon_check_circle_12;
    case NoticeIcon::Alert: return &icon_alert_12;
    case NoticeIcon::Shield: return &icon_shield_12;
    case NoticeIcon::Clock: return &icon_clock_12;
    case NoticeIcon::Bell: return &icon_bell_12;
    case NoticeIcon::Terminal: return &icon_terminal_12;
    default: return &icon_info_12;
  }
}

void notice_tick() {
  uint8_t arrivals = notice_take_arrivals();
  if (arrivals & NOTICE_ARRIVAL_WAKE) power_wake();
  if ((arrivals & NOTICE_ARRIVAL_SOUND) && settings().sound) speaker_chime();
  NoticeBoard &board = notices();
  board.expire(millis());  // mesmo relógio do notice_server (posted_ms)
  statuses().expire(millis());  // a Vitrine vê pela versão
  bool want_showcase = nav_current() == Screen::Showcase;
  if (!s_drawn || board.version() != s_drawn_version ||
      (board.size() > 0 && want_showcase != s_in_showcase)) {
    redraw();
  }
}
