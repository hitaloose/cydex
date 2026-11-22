// 19 Hall da Fama (canvas: HallDaFama.dc.html): os times de todos os saves.
//   cabeçalho: voltar, "Hall da Fama", chip do filtro (abre a Escolha 05g)
//   cards de 50 px (3 por página): troféu + "#n" à esquerda; treinador ·
//   jogo, "1ª vitória …" no #1 ou "Nv máx" nos outros, e os 6 ícones 24
//   rodapé: pager_bar (meio: treinador · jogo do 1º card + "pág X de Y")
// Ordem: por save (ordem do saves.cache) e, dentro dele, do mais novo pro
// mais velho. Times iguais em dois saves entram uma vez (save_store_hof_list).
// Memória: a lista (2 B por time) é estática; cada página lê os 3 times do
// SD e carrega 18 ícones 24.
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
constexpr uint16_t kMaxRefs = 16 * SAVE_HOF_MAX;

struct Mem {
  HofRef refs[kMaxRefs];
  Sprite icons[kPerPage * 6];
};
static_assert(sizeof(Mem) <= SAVES_SCREEN_MEM, "Hall da Fama não cabe na área das telas");
Mem *s_m;
#define s_refs (s_m->refs)
#define s_icons (s_m->icons)
uint16_t s_count;
PagerBar s_bar;
lv_obj_t *s_body;
char s_range[40];

// Escolha do filtro: 0 = todos; depois um item por save com time.
uint8_t s_choice_saves[16];
uint16_t s_choice_counts[17];
uint8_t s_choice_n;
char s_choice_buf[48];

void on_back(lv_event_t *) { nav_go(Screen::Menu); }

void free_icons() {
  for (Sprite &s : s_icons) sprite_free(&s);
}

void on_open(lv_event_t *e) {
  SavesState &st = saves_state();
  st.hof_index = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
  nav_go(Screen::HallTeam);
}

void card(lv_obj_t *parent, lv_coord_t y, uint16_t index, uint8_t k) {
  const HofRef r = s_refs[index];
  SaveHead hd;
  if (!save_store_head(r.save, &hd)) return;
  SaveMon team[6];
  uint8_t n = save_store_open(r.save) ? save_store_hof_team(r.team, team) : 0;
  bool latest = r.team + 1 == hd.hof_count;

  lv_obj_t *box = ui_box(parent);
  lv_obj_set_pos(box, kPad, y);
  lv_obj_set_size(box, 320 - 2 * kPad, kCardH);
  lv_obj_set_style_radius(box, 8, 0);
  lv_obj_set_style_bg_color(box, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, c(theme::LINE), 0);
  lv_obj_set_style_clip_corner(box, true, 0);
  lv_obj_set_style_bg_color(box, c(theme::SURFACE_2), LV_STATE_PRESSED);
  ui_on_click(box, on_open, (void *)(uintptr_t)index);

  lv_obj_t *left = ui_box(box);
  lv_obj_set_size(left, 48, LV_PCT(100));
  lv_obj_set_style_bg_color(left, c(theme::SURFACE_DIM), 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(left, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_border_width(left, 1, 0);
  lv_obj_set_style_border_color(left, c(theme::LINE), 0);
  lv_obj_align(ui_icon(left, &icon_trophy_14, latest ? theme::NOTICE_IDLE : theme::MUTED), LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_t *num = ui_label(left, &font_14_700, theme::TEXT, "");
  lv_label_set_text_fmt(num, "#%u", r.team + 1);
  lv_obj_align(num, LV_ALIGN_TOP_MID, 0, 24);

  lv_obj_t *top = ui_box(box);
  lv_obj_set_pos(top, 56, 4);
  lv_obj_set_size(top, 320 - 2 * kPad - 2 - 56 - 8, 16);
  lv_obj_set_style_pad_column(top, 6, 0);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *dot = ui_box(top);
  lv_obj_set_size(dot, 8, 8);
  lv_obj_set_style_radius(dot, 4, 0);
  lv_obj_set_style_bg_color(dot, c(save_game_color(hd.game)), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  char buf[48];
  snprintf(buf, sizeof(buf), tr(STR_HOF_SAVE_FMT), hd.name, save_game_name(hd.game));
  if (k == 0) snprintf(s_range, sizeof(s_range), tr(STR_HOF_SAVE_FMT), hd.name, save_game_name(hd.game));
  lv_obj_t *who = ui_label(top, &font_12_700, theme::TEXT, buf);
  lv_label_set_long_mode(who, LV_LABEL_LONG_DOT);
  lv_obj_set_flex_grow(who, 1);
  if (r.team == 0 && hd.first_hof) {
    char t[20];
    format_packed_time(hd.first_hof, t, sizeof(t));
    snprintf(buf, sizeof(buf), tr(STR_HOF_FIRST_FMT), t);
  } else {
    uint8_t max = 0;
    for (uint8_t i = 0; i < n; i++) max = team[i].level > max ? team[i].level : max;
    snprintf(buf, sizeof(buf), tr(STR_HOF_MAX_LEVEL_FMT), max);
  }
  ui_label(top, &font_12_500, theme::MUTED, buf);

  for (uint8_t i = 0; i < 6; i++) {
    lv_coord_t x = 56 + i * (24 + 3), iy = 22;
    if (i >= n) {
      lv_obj_t *empty = ui_box(box);
      lv_obj_set_pos(empty, x, iy);
      lv_obj_set_size(empty, 24, 24);
      lv_obj_set_style_radius(empty, 3, 0);
      lv_obj_set_style_border_width(empty, 1, 0);
      lv_obj_set_style_border_color(empty, c(theme::ROW_DIVIDER), 0);
      continue;
    }
    lv_obj_t *img = ui_mon_sprite(box, team[i].species, SpriteSize::S24, &s_icons[k * 6 + i], true);
    lv_obj_set_pos(img, x, iy);
  }
}

void render() {
  free_icons();
  lv_obj_clean(s_body);
  const Pager &p = s_bar.pager;
  s_range[0] = 0;
  for (uint8_t k = 0; k < p.items_on_page(); k++) {
    card(s_body, kPad + k * (kCardH + kGap), (uint16_t)(p.first_index() + k), k);
  }
  pager_bar_refresh(&s_bar, s_range);
  saves_state().hof_page = p.page;
}

void on_page(bool holding) {
  if (holding) {  // segurando a seta: só o número da página (cada página lê o SD)
    pager_bar_refresh(&s_bar, "");
    return;
  }
  render();
}

// ---------- filtro (05g) ----------

const char *choice_label(uint8_t i) {
  if (i == 0) return tr(STR_HOF_ALL_SAVES);
  SaveHead hd;
  if (!save_store_head(s_choice_saves[i - 1], &hd)) return "?";
  snprintf(s_choice_buf, sizeof(s_choice_buf), tr(STR_HOF_SAVE_FMT), hd.name, save_game_name(hd.game));
  return s_choice_buf;
}

const char *choice_hint(uint8_t i) {
  snprintf(s_choice_buf, sizeof(s_choice_buf), "%u", s_choice_counts[i]);
  return s_choice_buf;
}

void choice_pick(uint8_t i) {
  SavesState &st = saves_state();
  st.hof_filter = i == 0 ? 0xFF : s_choice_saves[i - 1];
  st.hof_page = 0;
}

void on_filter(lv_event_t *) {
  s_choice_n = 0;
  uint8_t selected = 0;
  SaveHead hd;
  for (uint8_t i = 0; i < save_store_count() && s_choice_n < 16; i++) {
    if (!save_store_head(i, &hd) || hd.hof_count == 0) continue;
    if (saves_state().hof_filter == i) selected = s_choice_n + 1;
    s_choice_saves[s_choice_n] = i;
    s_choice_counts[++s_choice_n] = save_store_hof_list(nullptr, 0, i);
  }
  s_choice_counts[0] = save_store_hof_total();
  ChoiceSpec spec{};
  spec.title = STR_TITLE_HOF;
  spec.count = (uint8_t)(s_choice_n + 1);
  spec.selected = selected;
  spec.label = choice_label;
  spec.hint = choice_hint;
  spec.pick = choice_pick;
  spec.back = Screen::HallOfFame;
  settings_choice_open(spec);
}

void filter_chip(lv_obj_t *h) {
  SavesState &st = saves_state();
  lv_obj_t *pill = ui_box(h);
  lv_obj_set_size(pill, LV_SIZE_CONTENT, 24);
  lv_obj_set_style_pad_left(pill, 10, 0);
  lv_obj_set_style_pad_right(pill, 6, 0);
  lv_obj_set_style_radius(pill, 12, 0);
  lv_obj_set_style_bg_color(pill, c(theme::SURFACE_2), 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(pill, 1, 0);
  lv_obj_set_style_border_color(pill, c(theme::LINE), 0);
  lv_obj_set_style_pad_column(pill, 4, 0);
  lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  char buf[48];
  if (st.hof_filter == 0xFF) {
    snprintf(buf, sizeof(buf), tr(STR_HOF_ALL_FMT), s_count);
  } else {
    SaveHead hd;
    if (save_store_head(st.hof_filter, &hd)) snprintf(buf, sizeof(buf), "%s · %u", hd.name, s_count);
    else snprintf(buf, sizeof(buf), "%u", s_count);
  }
  lv_obj_t *l = ui_label(pill, &font_12_600, theme::TEXT, buf);
  lv_obj_set_style_max_width(l, 130, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  ui_icon(pill, &icon_forward_12, theme::MUTED);
  lv_obj_set_style_translate_x(pill, -6, 0);
  lv_obj_set_ext_click_area(pill, 6);
  ui_on_click(pill, on_filter);
}

void empty_state(lv_obj_t *scr) {
  lv_obj_t *col = ui_box(scr);
  lv_obj_set_pos(col, 20, theme::HEADER_H);
  lv_obj_set_size(col, 280, 240 - theme::HEADER_H);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 10, 0);
  ui_icon(col, &icon_trophy_20, theme::FAINT);
  lv_obj_t *l = ui_label(col, &font_12_500, theme::MUTED, tr(STR_HOF_EMPTY));
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}

void on_delete(lv_event_t *) {
  if (s_m) free_icons();
  saves_screen_mem_release();
  s_m = nullptr;
}

}  // namespace

uint16_t hall_list_build(HofRef *out, uint16_t max) {
  uint16_t n = save_store_hof_list(out, max, saves_state().hof_filter);
  return n > max ? max : n;
}

lv_obj_t *hall_screen_create() {
  s_m = (Mem *)saves_screen_mem_take(sizeof(Mem));
  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);
  lv_obj_t *h = ui_header(scr, tr(STR_TITLE_HOF), on_back);
  if (!s_m) return scr;
  s_count = hall_list_build(s_refs, kMaxRefs);
  if (s_count == 0 && saves_state().hof_filter != 0xFF) {  // o save do filtro sumiu
    saves_state().hof_filter = 0xFF;
    s_count = hall_list_build(s_refs, kMaxRefs);
  }
  if (s_count == 0) {
    empty_state(scr);
    return scr;
  }
  ui_header_spacer(h);
  filter_chip(h);

  s_body = ui_box(scr);
  lv_obj_set_pos(s_body, 0, theme::HEADER_H);
  lv_obj_set_size(s_body, 320, 240 - theme::HEADER_H - theme::PAGER_H);
  s_bar = PagerBar();
  s_bar.on_page = on_page;
  s_bar.on_center = nullptr;
  pager_bar_create(&s_bar, scr);
  s_bar.pager.reset(s_count, kPerPage);
  s_bar.pager.go_to_page(saves_state().hof_page);
  render();
  return scr;
}
