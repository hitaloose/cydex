#include "saves_ui.h"

#include <cstdio>
#include <cstring>

#include "i18n/i18n.h"
#include "ui/catalog.h"
#include "ui/icons/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

SavesState &saves_state() {
  static SavesState s;
  return s;
}

static void *s_mem;

void *saves_screen_mem_take(size_t bytes) {
  saves_screen_mem_release();
  s_mem = sprite_arena_borrow(bytes);
  if (s_mem) memset(s_mem, 0, bytes);
  return s_mem;
}

void saves_screen_mem_release() {
  sprite_arena_return(s_mem);
  s_mem = nullptr;
}

void format_playtime(uint16_t h, uint8_t m, char *out, size_t size) {
  snprintf(out, size, tr(STR_PLAYTIME_FMT), h, m);
}

void format_packed_time(uint32_t packed, char *out, size_t size) {
  format_playtime((uint16_t)(packed >> 16), (uint8_t)(packed >> 8), out, size);
}

void format_thousands(uint32_t v, char *out, size_t size) {
  // Milhar: o contrário do decimal do idioma ("," -> ".").
  char sep = tr(STR_DECIMAL_SEP)[0] == ',' ? '.' : ',';
  char digits[12];
  int n = snprintf(digits, sizeof(digits), "%lu", (unsigned long)v);
  size_t o = 0;
  for (int i = 0; i < n && o + 2 < size; i++) {
    if (i > 0 && (n - i) % 3 == 0) out[o++] = sep;
    out[o++] = digits[i];
  }
  out[o] = 0;
}

const char *nature_name(uint8_t nature) { return tr((StrId)(STR_NATURE_HARDY + nature % 25)); }

lv_obj_t *ui_game_chip(lv_obj_t *parent, SaveGame g) {
  lv_obj_t *chip = ui_chip(parent, save_game_name(g),
                           save_game_color(g), theme::ON_ACCENT, 7);
  lv_obj_set_height(chip, 18);
  lv_obj_set_style_radius(chip, 9, 0);
  return chip;
}

lv_obj_t *ui_badges(lv_obj_t *parent, uint16_t badges, uint8_t total, lv_coord_t size, lv_coord_t gap,
                    lv_coord_t row_gap) {
  if (total > SAVE_BADGES_MAX) total = SAVE_BADGES_MAX;
  uint8_t rows = total > 8 ? 2 : 1;  // 16 (HG/SS): Johto em cima, Kanto embaixo
  lv_obj_t *box = ui_box(parent);
  lv_obj_set_size(box, 8 * size + 7 * gap, rows * size + (rows - 1) * row_gap);
  lv_obj_set_style_pad_column(box, gap, 0);
  lv_obj_set_style_pad_row(box, row_gap, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  const lv_img_dsc_t *on, *off;
  if (size >= 10) on = &icon_badge_10, off = &icon_badge_line_10;
  else if (size >= 8) on = &icon_badge_8, off = &icon_badge_line_8;
  else if (size >= 7) on = &icon_badge_7, off = &icon_badge_line_7;
  else on = &icon_badge_6, off = &icon_badge_line_6;
  for (int i = 0; i < total; i++) {
    bool got = (badges >> i) & 1;
    ui_icon(box, got ? on : off, got ? theme::ACCENT : theme::FAINT);
  }
  return box;
}

lv_obj_t *ui_gender(lv_obj_t *parent, uint8_t gender, lv_coord_t size) {
  constexpr uint32_t kFemale = 0xFF7FB0, kMale = 0x5AA8FF;  // Psíquico / Água
  if (gender == 1) return ui_icon(parent, size >= 13 ? &icon_female_13 : &icon_female_12, kFemale);
  return ui_icon(parent, size >= 13 ? &icon_male_13 : &icon_male_12, kMale);
}

static const lv_img_dsc_t *dash_for(SpriteSize size) {
  switch (size) {
    case SpriteSize::S96: return &icon_dash_box_96;
    case SpriteSize::S64: return &icon_dash_box_64;
    case SpriteSize::S24: return &icon_dash_box_24;
    default: return &icon_dash_box_48;
  }
}

void ui_mon_sprite_load(lv_obj_t *img, uint16_t n, SpriteSize size, Sprite *slot) {
  if (n && sprite_load(n, size, slot)) {
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_TRANSP, 0);
    lv_img_set_src(img, &slot->dsc);
  }
}

lv_obj_t *ui_mon_sprite(lv_obj_t *parent, uint16_t n, SpriteSize size, Sprite *slot, bool load) {
  lv_obj_t *img = lv_img_create(parent);
  lv_img_set_src(img, dash_for(size));
  lv_obj_set_style_img_recolor(img, c(theme::FAINT_2), 0);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  if (load && slot) ui_mon_sprite_load(img, n, size, slot);
  return img;
}

lv_obj_t *ui_mon_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                      const MonCardSpec &m, Sprite *slot, lv_event_cb_t on_tap, void *user) {
  lv_obj_t *card = ui_box(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_bg_color(card, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, c(theme::SURFACE_2), LV_STATE_PRESSED);
  if (on_tap) ui_on_click(card, on_tap, user);

  lv_obj_t *img = ui_mon_sprite(card, m.egg ? 0 : m.species, SpriteSize::S48, slot, !m.egg);
  lv_obj_set_pos(img, 3, 3);

  lv_obj_t *lv = ui_label(card, &font_12_500, theme::MUTED, tr(STR_SAVE_LEVEL));
  lv_obj_align(lv, LV_ALIGN_TOP_RIGHT, -6, 5);
  lv_obj_t *num = ui_label(card, &font_16_700, theme::TEXT, "");
  if (m.egg) lv_label_set_text(num, "?");
  else lv_label_set_text_fmt(num, "%u", m.level);
  lv_obj_align(num, LV_ALIGN_TOP_RIGHT, -6, 19);
  if (m.shiny) lv_obj_align(ui_icon(card, &icon_star_13, theme::NOTICE_IDLE), LV_ALIGN_TOP_RIGHT, -6, 38);

  lv_coord_t text_w = w - 12;
  lv_obj_t *nick = ui_label(card, &font_12_700, theme::TEXT, m.egg ? tr(STR_MEMBER_EGG) : m.nick);
  lv_obj_set_width(nick, text_w);
  lv_label_set_long_mode(nick, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(nick, 6, 3 + 48 + 2);

  if (m.sub) {
    lv_obj_t *sub = ui_label(card, &font_12_500, theme::MUTED, m.sub);
    lv_obj_set_width(sub, text_w);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(sub, 6, 3 + 48 + 2 + 15);
  } else if (m.hp_max) {
    lv_obj_t *bar = ui_progress(card, text_w, 3);
    lv_obj_set_pos(bar, 6, 3 + 48 + 2 + 16);
    uint32_t pct = (uint32_t)m.hp * 100 / m.hp_max;
    uint32_t color = pct > 50 ? theme::NOTICE_DONE : pct > 20 ? theme::NOTICE_IDLE : theme::ACCENT;
    lv_obj_set_style_bg_color(bar, c(color), LV_PART_INDICATOR);
    ui_progress_set(bar, m.hp, m.hp_max);
  }
  return card;
}

lv_obj_t *ui_mon_empty(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                       bool dashed) {
  lv_obj_t *box = ui_box(parent);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, w, h);
  lv_obj_set_style_radius(box, 8, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, c(dashed ? theme::FAINT_2 : theme::ROW_DIVIDER), 0);
  if (dashed) lv_obj_center(ui_label(box, &font_12_500, theme::FAINT, tr(STR_SAVE_EMPTY_SLOT)));
  return box;
}

void open_species_from(uint16_t n, Screen back) {
  if (!n) return;
  CatalogState &cs = catalog_state();
  cs.selected = n;
  cs.tab = DetailTab::Info;
  cs.back = back;
  nav_go(Screen::Detail);
}
