// 16 Saves e 16a sem saves (canvas: Saves.dc.html, SavesVazio.dc.html).
//   cabeçalho: voltar, "Saves", "N saves" à direita
//   cards de 50 px (3 por página): faixa na cor do jogo, treinador + gênero,
//   chip do jogo, "tempo · dex x/386" e as 8 insígnias. Não reconhecido:
//   card tracejado, sem toque.
//   rodapé: pager_bar só com "pág X de Y"
// Tudo vem do saves.cache (sem abrir os saves).
#include <cstdio>
#include <cstring>

#include "i18n/i18n.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/pager_bar.h"
#include "ui/saves_ui.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr uint8_t kPerPage = 3;
constexpr lv_coord_t kCardH = 50, kGap = 4, kPad = 6;

PagerBar s_bar;
lv_obj_t *s_body;

void on_back(lv_event_t *) { nav_go(Screen::Menu); }

void on_open(lv_event_t *e) {
  SavesState &st = saves_state();
  st.save = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  st.tab = SaveTab::Summary;
  st.box = 0;
  st.box_slot = 0xFF;
  nav_go(Screen::Save);
}

void card(lv_obj_t *parent, lv_coord_t y, uint8_t index) {
  SaveHead s;
  bool ok = save_store_head(index, &s);
  char file[SAVES_CACHE_NAME_SIZE];
  if (!ok && !save_store_file(index, file, sizeof(file))) return;

  lv_obj_t *box = ui_box(parent);
  lv_obj_set_pos(box, kPad, y);
  lv_obj_set_size(box, 320 - 2 * kPad, kCardH);
  lv_obj_set_style_radius(box, 8, 0);

  if (!ok) {
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, c(theme::FAINT_2), 0);
    lv_obj_t *n = ui_label(box, &font_14_600, theme::MUTED, file);
    lv_obj_set_width(n, 320 - 2 * kPad - 20);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(n, 10, 7);
    lv_obj_set_pos(ui_label(box, &font_12_500, theme::FAINT, tr(STR_SAVES_UNKNOWN)), 10, 27);
    return;
  }

  lv_obj_set_style_bg_color(box, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, c(theme::LINE), 0);
  lv_obj_set_style_clip_corner(box, true, 0);
  lv_obj_set_style_bg_color(box, c(theme::SURFACE_2), LV_STATE_PRESSED);
  ui_on_click(box, on_open, (void *)(uintptr_t)index);

  lv_obj_t *stripe = ui_box(box);
  lv_obj_set_size(stripe, 4, LV_PCT(100));
  lv_obj_set_style_bg_color(stripe, c(save_game_color(s.game)), 0);
  lv_obj_set_style_bg_opa(stripe, LV_OPA_COVER, 0);

  lv_obj_t *top = ui_box(box);
  lv_obj_set_pos(top, 13, 5);
  lv_obj_set_size(top, 320 - 2 * kPad - 2 - 13 - 10, 18);
  lv_obj_set_style_pad_column(top, 6, 0);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_label(top, &font_14_700, theme::TEXT, s.name);
  ui_gender(top, s.gender, 12);
  ui_header_spacer(top);
  ui_game_chip(top, s.game);

  char time[20], sub[48];
  format_playtime(s.hours, s.minutes, time, sizeof(time));
  snprintf(sub, sizeof(sub), tr(STR_SAVES_SUB_FMT), time, s.caught, s.dex_total);
  lv_obj_set_pos(ui_label(box, &font_12_500, theme::MUTED, sub), 13, 27);
  lv_obj_t *badges = ui_badges(box, s.badges, 7, 3);
  lv_obj_align(badges, LV_ALIGN_TOP_RIGHT, -10, 31);
}

void render() {
  lv_obj_clean(s_body);
  const Pager &p = s_bar.pager;
  for (uint8_t k = 0; k < p.items_on_page(); k++) {
    card(s_body, kPad + k * (kCardH + kGap), (uint8_t)(p.first_index() + k));
  }
  pager_bar_refresh(&s_bar, "");
  saves_state().list_page = (uint8_t)p.page;
}

void on_page(bool) { render(); }

void empty_state(lv_obj_t *scr) {
  lv_obj_t *col = ui_box(scr);
  lv_obj_set_pos(col, 20, theme::HEADER_H);
  lv_obj_set_size(col, 280, 240 - theme::HEADER_H);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 10, 0);

  lv_obj_t *frame = ui_box(col);
  lv_obj_set_size(frame, 56, 56);
  lv_obj_set_style_radius(frame, 12, 0);
  lv_obj_set_style_border_width(frame, 1, 0);
  lv_obj_set_style_border_color(frame, c(theme::FAINT_2), 0);
  lv_obj_center(ui_icon(frame, &icon_cartridge_28, theme::FAINT));

  ui_label(col, &font_16_700, theme::TEXT, tr(STR_SAVES_EMPTY_TITLE));
  lv_obj_t *body = ui_label(col, &font_12_500, theme::MUTED, tr(STR_SAVES_EMPTY_BODY));
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
  ui_label_clamp(body, 280, 3);
}

}  // namespace

lv_obj_t *saves_screen_create() {
  lv_obj_t *scr = ui_screen();
  lv_obj_t *h = ui_header(scr, tr(STR_TITLE_SAVES), on_back);
  uint8_t count = save_store_count();
  if (count == 0) {
    empty_state(scr);
    return scr;
  }
  ui_header_spacer(h);
  lv_obj_t *n = ui_label(h, &font_12_600, theme::MUTED, "");
  uint8_t ok = save_store_ok_count();
  if (ok == 1) lv_label_set_text(n, tr(STR_SAVES_COUNT_ONE));
  else lv_label_set_text_fmt(n, tr(STR_SAVES_COUNT_FMT), ok);
  lv_obj_set_style_pad_right(n, 10, 0);

  s_body = ui_box(scr);
  lv_obj_set_pos(s_body, 0, theme::HEADER_H);
  lv_obj_set_size(s_body, 320, 240 - theme::HEADER_H - theme::PAGER_H);

  s_bar = PagerBar();
  s_bar.on_page = on_page;
  s_bar.on_center = nullptr;
  pager_bar_create(&s_bar, scr);
  lv_obj_add_flag(s_bar.range, LV_OBJ_FLAG_HIDDEN);  // só "pág X de Y"
  s_bar.pager.reset(count, kPerPage);
  s_bar.pager.go_to_page(saves_state().list_page);
  render();
  return scr;
}
