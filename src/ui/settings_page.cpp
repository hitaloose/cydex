#include "settings_page.h"

#include "logic/RowPages.h"
#include "ui/pager_bar.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kFullH = 240 - theme::HEADER_H;
constexpr lv_coord_t kPagedH = kFullH - theme::PAGER_H;
constexpr uint8_t kMaxRows = RowPages::MAX_PAGES * 6;

const SettingsRow *s_rows;
uint8_t *s_page;
RowPages s_pages;
lv_obj_t *s_body;
PagerBar s_bar;

RowPages paginate(const SettingsRow *rows, uint8_t count) {
  uint16_t heights[kMaxRows];
  for (uint8_t i = 0; i < count; i++) heights[i] = (uint16_t)rows[i].h;
  return row_pages(heights, count, kFullH, kPagedH);
}

lv_coord_t build_page() {
  lv_obj_clean(s_body);
  lv_coord_t y = 0;
  for (uint8_t i = s_pages.first(*s_page); i < s_pages.end(*s_page); i++) {
    s_rows[i].build(s_body, y, s_rows[i].h, i);
    y += s_rows[i].h;
  }
  return y;
}

void on_page(bool) {
  *s_page = (uint8_t)s_bar.pager.page;
  build_page();
  pager_bar_refresh(&s_bar, "");
}

void on_body_delete(lv_event_t *) { s_body = nullptr; }

}  // namespace

lv_coord_t settings_page(lv_obj_t *scr, const SettingsRow *rows, uint8_t count, uint8_t *page) {
  if (count > kMaxRows) count = kMaxRows;
  s_rows = rows;
  s_page = page;
  s_pages = paginate(rows, count);
  if (*s_page >= s_pages.count) *s_page = s_pages.count - 1;

  s_body = ui_box(scr);
  lv_obj_set_pos(s_body, 0, theme::HEADER_H);
  lv_obj_set_size(s_body, 320, s_pages.paged() ? kPagedH : kFullH);
  lv_obj_add_event_cb(s_body, on_body_delete, LV_EVENT_DELETE, nullptr);
  lv_coord_t used = build_page();

  if (s_pages.paged()) {
    s_bar = PagerBar();
    s_bar.on_page = on_page;
    s_bar.on_center = nullptr;
    pager_bar_create(&s_bar, scr);
    lv_obj_add_flag(s_bar.range, LV_OBJ_FLAG_HIDDEN);  // só "pág X de Y"
    s_bar.pager.reset(s_pages.count, 1);
    s_bar.pager.go_to_page(*s_page);
    pager_bar_refresh(&s_bar, "");
  }
  return theme::HEADER_H + used;
}

uint8_t settings_page_of(const SettingsRow *rows, uint8_t count, uint8_t row) {
  if (count > kMaxRows) count = kMaxRows;
  return paginate(rows, count).page_of(row);
}
