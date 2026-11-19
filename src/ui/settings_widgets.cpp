#include "settings_widgets.h"

#include "ui/icons/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "version.h"

lv_obj_t *set_row(lv_obj_t *scr, lv_coord_t y, lv_coord_t h, bool divider) {
  lv_obj_t *row = ui_box(scr);
  lv_obj_set_pos(row, 0, y);
  lv_obj_set_size(row, 320, h);
  lv_obj_set_style_pad_hor(row, 10, 0);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (divider) {
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, c(theme::ROW_DIVIDER), 0);
  }
  return row;
}

lv_obj_t *set_label(lv_obj_t *row, StrId text) {
  lv_obj_t *l = ui_label(row, &font_14_600, theme::TEXT, tr(text));
  ui_header_spacer(row);
  return l;
}

// ---------- slider ----------

lv_obj_t *set_slider(lv_obj_t *row, int32_t min, int32_t max, int32_t value,
                     lv_event_cb_t on_change, lv_event_cb_t on_release) {
  // Invólucro 130x24 (o input do canvas); o trilho tem 114 pra o knob de 16
  // não passar da borda (o LVGL 8 não tem margin).
  lv_obj_t *wrap = ui_box(row);
  lv_obj_set_size(wrap, 130, 24);
  lv_obj_t *s = lv_slider_create(wrap);
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, 130 - 16, 6);
  lv_obj_center(s);
  lv_obj_set_style_radius(s, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s, c(theme::SURFACE_2), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(s, 3, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s, c(theme::ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_bg_color(s, c(theme::ACCENT), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_pad_all(s, 5, LV_PART_KNOB);  // 6 + 2x5 = knob de 16
  lv_obj_set_style_anim_time(s, 0, 0);
  lv_obj_set_ext_click_area(s, 12);  // alvo vertical de 30 px
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_slider_set_range(s, min, max);
  lv_slider_set_value(s, value, LV_ANIM_OFF);
  if (on_change) lv_obj_add_event_cb(s, on_change, LV_EVENT_VALUE_CHANGED, nullptr);
  if (on_release) lv_obj_add_event_cb(s, on_release, LV_EVENT_RELEASED, nullptr);
  return s;
}

// ---------- interruptor ----------

static void style_switch(lv_obj_t *sw, bool on) {
  lv_obj_set_style_bg_color(sw, c(on ? theme::ACCENT : theme::FAINT_2), 0);
  lv_obj_t *knob = lv_obj_get_child(sw, 0);
  lv_obj_align(knob, on ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, on ? -3 : 3, 0);
  lv_obj_set_user_data(sw, (void *)(uintptr_t)on);
}

static void on_switch_click(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_current_target(e);
  style_switch(sw, !set_switch_get(sw));
}

lv_obj_t *set_switch(lv_obj_t *row, bool on, lv_event_cb_t on_toggle) {
  lv_obj_t *sw = ui_box(row);
  lv_obj_set_size(sw, 44, 24);
  lv_obj_set_style_radius(sw, 12, 0);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
  lv_obj_set_ext_click_area(sw, 6);
  lv_obj_t *knob = ui_box(sw);
  lv_obj_set_size(knob, 18, 18);
  lv_obj_set_style_radius(knob, 9, 0);
  lv_obj_set_style_bg_color(knob, c(theme::BG), 0);
  lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
  style_switch(sw, on);
  // Primeiro pinta, depois avisa quem chamou (que lê o estado novo).
  ui_on_click(sw, on_switch_click);
  if (on_toggle) lv_obj_add_event_cb(sw, on_toggle, LV_EVENT_CLICKED, nullptr);
  return sw;
}

bool set_switch_get(lv_obj_t *sw) { return (bool)(uintptr_t)lv_obj_get_user_data(sw); }

// ---------- segmentado ----------

void set_segmented_select(lv_obj_t *seg, uint8_t selected) {
  uint32_t n = lv_obj_get_child_cnt(seg);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *b = lv_obj_get_child(seg, i);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    bool on = i == selected;
    lv_obj_set_style_bg_opa(b, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(l, on ? &font_12_700 : &font_12_600, 0);
    lv_obj_set_style_text_color(l, c(on ? theme::ON_ACCENT : theme::MUTED), 0);
  }
}

lv_obj_t *set_segmented(lv_obj_t *row, const StrId *labels, uint8_t count, uint8_t selected,
                        lv_event_cb_t on_pick, lv_coord_t pad_hor) {
  lv_obj_t *seg = ui_box(row);
  lv_obj_set_size(seg, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(seg, 8, 0);
  lv_obj_set_style_bg_color(seg, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(seg, 1, 0);
  lv_obj_set_style_border_color(seg, c(theme::LINE), 0);
  lv_obj_set_style_pad_all(seg, 2, 0);
  lv_obj_set_style_pad_column(seg, 2, 0);
  lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
  for (uint8_t i = 0; i < count; i++) {
    lv_obj_t *b = ui_box(seg);
    lv_obj_set_size(b, LV_SIZE_CONTENT, 26);
    lv_obj_set_style_pad_hor(b, pad_hor, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_bg_color(b, c(theme::ACCENT), 0);
    lv_obj_set_ext_click_area(b, 3);  // 26 + borda/padding do grupo = 32
    lv_obj_t *l = ui_label(b, &font_12_600, theme::MUTED, tr(labels[i]));
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    ui_on_click(b, on_pick, (void *)(uintptr_t)i);
  }
  set_segmented_select(seg, selected);
  return seg;
}

// ---------- passo ----------

static lv_obj_t *step_button(lv_obj_t *row, const char *glyph, lv_event_cb_t cb, intptr_t dir) {
  lv_obj_t *b = ui_box(row);
  lv_obj_set_size(b, 32, 28);
  lv_obj_set_style_radius(b, 6, 0);
  lv_obj_set_style_bg_color(b, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_color(b, c(theme::SURFACE_2), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, c(theme::LINE), 0);
  lv_obj_set_ext_click_area(b, 3);
  lv_obj_center(ui_label(b, &font_16_700, theme::TEXT, glyph));
  ui_on_click(b, cb, (void *)dir);
  return b;
}

Stepper set_stepper(lv_obj_t *row, lv_coord_t value_w, lv_event_cb_t on_step) {
  lv_obj_set_style_pad_column(row, 6, 0);
  Stepper st;
  st.minus = step_button(row, "−", on_step, -1);
  st.value = ui_label(row, &font_14_700, theme::TEXT, "");
  lv_obj_set_width(st.value, value_w);
  lv_obj_set_style_text_align(st.value, LV_TEXT_ALIGN_CENTER, 0);
  st.plus = step_button(row, "+", on_step, 1);
  return st;
}

static void set_step_enabled(lv_obj_t *b, bool enabled) {
  lv_obj_set_style_text_color(lv_obj_get_child(b, 0), c(enabled ? theme::TEXT : theme::FAINT_2), 0);
  if (enabled) lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  else lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
}

void set_stepper_update(const Stepper &st, const char *text, bool at_min, bool at_max) {
  lv_label_set_text(st.value, text);
  set_step_enabled(st.minus, !at_min);
  set_step_enabled(st.plus, !at_max);
}

// ---------- linha "›" ----------

lv_obj_t *set_link_row(lv_obj_t *scr, lv_coord_t y, lv_coord_t h, const lv_img_dsc_t *icon,
                       StrId label, const char *summary, lv_event_cb_t on_click, bool divider) {
  lv_obj_t *row = set_row(scr, y, h, divider);
  lv_obj_set_style_pad_left(row, 10, 0);
  lv_obj_set_style_pad_right(row, 6, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_style_bg_color(row, c(theme::SURFACE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
  ui_icon(row, icon, theme::ACCENT);
  ui_label(row, &font_14_600, theme::TEXT, tr(label));
  ui_header_spacer(row);
  ui_label(row, &font_12_500, theme::MUTED, summary);
  ui_icon(row, &icon_forward_18, theme::MUTED);
  ui_on_click(row, on_click);
  return row;
}

void set_header_version(lv_obj_t *header) {
  lv_obj_set_style_pad_right(header, 10, 0);
  ui_header_spacer(header);
  ui_label(header, &font_12_500, theme::MUTED, "CYDex v" CYDEX_VERSION);
}
