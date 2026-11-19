#include "pager_bar.h"

#include "i18n/i18n.h"
#include "ui/icons/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

static void step(PagerBar *b, int8_t dir, bool holding) {
  bool changed = dir < 0 ? b->pager.prev() : b->pager.next();
  if (changed && b->on_page) b->on_page(holding);
}

static void arrow_event(lv_event_t *e) {
  PagerBar *b = (PagerBar *)lv_event_get_user_data(e);
  lv_obj_t *target = lv_event_get_current_target(e);
  int8_t dir = target == b->prev ? -1 : 1;
  uint32_t now = lv_tick_get();

  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      b->hold_dir = dir;
      b->hold.press(now);
      step(b, dir, true);
      break;
    case LV_EVENT_PRESSING:
      if (b->hold.poll(now)) step(b, dir, true);
      break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
      if (b->hold.active) {
        b->hold.release();
        if (b->on_page) b->on_page(false);  // soltou: página final, carga completa
      }
      break;
    default:
      break;
  }
}

static void center_event(lv_event_t *e) {
  PagerBar *b = (PagerBar *)lv_event_get_user_data(e);
  if (b->on_center) b->on_center();
}

static lv_obj_t *arrow(PagerBar *b, lv_obj_t *parent, const lv_img_dsc_t *icon) {
  lv_obj_t *a = ui_box(parent);
  lv_obj_set_size(a, theme::PAGER_BTN_W, LV_PCT(100));
  lv_obj_t *i = ui_icon(a, icon, theme::TEXT);
  lv_obj_center(i);
  lv_obj_add_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(a, arrow_event, LV_EVENT_ALL, b);
  return a;
}

void pager_bar_create(PagerBar *b, lv_obj_t *screen) {
  b->hold = HoldRepeat();
  b->hold_dir = 0;

  b->root = ui_box(screen);
  lv_obj_set_size(b->root, LV_PCT(100), theme::PAGER_H);
  lv_obj_align(b->root, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(b->root, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(b->root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(b->root, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(b->root, 1, 0);
  lv_obj_set_style_border_color(b->root, c(theme::LINE), 0);
  lv_obj_set_flex_flow(b->root, LV_FLEX_FLOW_ROW);

  b->prev = arrow(b, b->root, &icon_prev_20);

  lv_obj_t *mid = ui_box(b->root);
  lv_obj_set_height(mid, LV_PCT(100));
  lv_obj_set_flex_grow(mid, 1);
  lv_obj_set_style_border_side(mid, (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT), 0);
  lv_obj_set_style_border_width(mid, 1, 0);
  lv_obj_set_style_border_color(mid, c(theme::LINE), 0);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  b->range = ui_label(mid, &font_14_700, theme::TEXT, "");
  b->page = ui_label(mid, &font_12_500, theme::MUTED, "");
  ui_on_click(mid, center_event, b);

  b->next = arrow(b, b->root, &icon_next_20);
}

static void set_arrow_enabled(lv_obj_t *a, bool on) {
  lv_obj_t *icon = lv_obj_get_child(a, 0);
  lv_obj_set_style_img_recolor(icon, c(on ? theme::TEXT : theme::FAINT_2), 0);
  if (on) lv_obj_add_flag(a, LV_OBJ_FLAG_CLICKABLE);
  else lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
}

void pager_bar_refresh(PagerBar *b, const char *range_text) {
  set_arrow_enabled(b->prev, b->pager.has_prev());
  set_arrow_enabled(b->next, b->pager.has_next());
  lv_label_set_text(b->range, range_text);
  if (b->pager.total == 0) {
    lv_label_set_text(b->page, tr(STR_PAGER_EMPTY));
  } else {
    lv_label_set_text_fmt(b->page, tr(STR_PAGER_PAGE_FMT), b->pager.page + 1, b->pager.page_count());
  }
}
