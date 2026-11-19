#include "notice_center.h"

#include <cstdio>

#include "data/Settings.h"
#include "hal/speaker.h"
#include "i18n/i18n.h"
#include "net/hook_server.h"
#include "ui/icons/icons.h"
#include "ui/nav.h"
#include "ui/power.h"
#include "ui/screens/screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "util/log.h"

// Só desenha o NoticeBoard (net/hook_server): quem põe, ordena e limpa é ele.
// Redesenha quando a versão do quadro muda ou quando entra/sai da Vitrine.

namespace {

constexpr lv_coord_t kBannerH = 56;

lv_obj_t *s_obj = nullptr;        // faixa ou balões (nullptr = nada desenhado)
bool s_in_showcase = false;       // o que está desenhado é dos balões
uint32_t s_drawn_version = 0;
bool s_drawn = false;
uint32_t s_seen_arrivals = 0;     // hook_arrivals() na última volta (detecta chegada)

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
  hook_notices().dismiss((uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

void clear_drawn() {
  if (s_obj) {
    lv_obj_t *o = s_obj;
    s_obj = nullptr;
    lv_obj_del(o);
  }
  s_drawn = false;
}

StrId title_of(NoticeKind k) {
  switch (k) {
    case NoticeKind::Permission: return STR_NOTICE_PERMISSION;
    case NoticeKind::Idle: return STR_NOTICE_IDLE;
    default: return STR_NOTICE_DONE;
  }
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

  uint32_t color = notice_color(n.kind);
  lv_obj_t *bar = ui_box(b);
  lv_obj_set_size(bar, 4, LV_PCT(100));
  lv_obj_set_style_bg_color(bar, c(color), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  ui_icon(b, notice_icon(n.kind, false), color);

  // Largura do texto: 320 - barra 4 - ícone 20 - 3 gaps de 10 - pad 10 - chip.
  lv_coord_t text_w = 320 - 4 - 20 - 30 - 10 - (more ? 40 : 0);
  lv_obj_t *col = ui_box(b);
  lv_obj_set_size(col, text_w, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(col, 1, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *t = ui_label(col, &font_14_700, theme::TEXT, tr(title_of(n.kind)));
  lv_obj_set_width(t, text_w);
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
  char sub[sizeof(n.project) + sizeof(n.message) + 8];
  if (n.message[0]) snprintf(sub, sizeof(sub), "%s · %s", n.project, n.message);
  else snprintf(sub, sizeof(sub), "%s", n.project);
  lv_obj_t *s = ui_label(col, &font_12_500, theme::MUTED, sub);
  ui_label_clamp(s, text_w, 2);

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
  const NoticeBoard &board = hook_notices();
  s_drawn_version = board.version();
  s_in_showcase = nav_current() == Screen::Showcase;
  s_drawn = true;
  if (board.size() == 0) return;

  lv_obj_t *o = nullptr;
  if (s_in_showcase) {
    Notice items[NOTICE_BOARD_CAP];
    uint8_t n = board.size();
    for (uint8_t i = 0; i < n; i++) items[i] = board.at(i);
    o = showcase_show_notices(items, n, on_dismiss);
  }
  if (!o) {
    s_in_showcase = false;
    o = banner(board.at(0), (uint8_t)(board.size() - 1));
  }
  s_obj = o;
  lv_obj_add_event_cb(o, on_obj_delete, LV_EVENT_DELETE, nullptr);
}

}  // namespace

uint32_t notice_color(NoticeKind k) {
  switch (k) {
    case NoticeKind::Permission: return theme::ACCENT;
    case NoticeKind::Idle: return theme::NOTICE_IDLE;
    default: return theme::NOTICE_DONE;
  }
}

const lv_img_dsc_t *notice_icon(NoticeKind k, bool small) {
  switch (k) {
    case NoticeKind::Permission: return small ? &icon_shield_16 : &icon_shield_20;
    case NoticeKind::Idle: return small ? &icon_clock_16 : &icon_clock_20;
    default: return small ? &icon_check_circle_16 : &icon_check_circle_20;
  }
}

void notice_tick() {
  if (hook_arrivals() != s_seen_arrivals) {
    s_seen_arrivals = hook_arrivals();
    power_wake();
    if (settings().sound) speaker_chime();
  }
  bool want_showcase = nav_current() == Screen::Showcase;
  const NoticeBoard &board = hook_notices();
  if (!s_drawn || board.version() != s_drawn_version ||
      (board.size() > 0 && want_showcase != s_in_showcase)) {
    redraw();
  }
}
