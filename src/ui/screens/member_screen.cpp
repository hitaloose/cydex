// 18 Membro (canvas: Membro.dc.html): uma criatura do time ou da caixa.
//   cabeçalho: voltar (Time ou Caixas), "Ana · Time 1/5" ou "Ana · CAIXA",
//   ‹ › pro vizinho (time: 1..n; caixa: próxima vaga ocupada)
//   Info: sprite 96 + selo (★ Brilhante ou Nv), apelido, espécie (toque abre
//   a Ficha), tipos, natureza, item, treinador
//   Status: valores reais; o que a natureza sobe em accent, o que desce em azul
//   Golpes: até 4 linhas com tipo, nome e PP (moves.bin)
#include <cstdio>
#include <cstring>

#include "data/NameRepo.h"
#include "data/SpeciesRepo.h"
#include "dex/Types.h"
#include "i18n/i18n.h"
#include "logic/MonStats.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/saves_ui.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kBodyY = theme::HEADER_H;
constexpr lv_coord_t kBodyH = 240 - theme::HEADER_H - theme::TABS_H;
constexpr uint8_t kTabCount = 3;
constexpr uint32_t kDown = 0x5AA8FF;  // = Água

struct Mem {
  SaveMon list[SAVE_BOX_SLOTS];
  SaveMon mon;
  Sprite sprite;
};
static_assert(sizeof(Mem) <= SAVES_SCREEN_MEM, "Membro não cabe na área das telas");
Mem *s_m;
#define s_list (s_m->list)
#define s_mon (s_m->mon)
#define s_sprite (s_m->sprite)
uint8_t s_count;  // vagas válidas em s_list (time) ou 30 (caixa)
char s_box_name[24];
SaveHead s_head;
uint8_t s_tab;
lv_obj_t *s_panels[kTabCount];
lv_obj_t *s_tabs[kTabCount];

const StrId kTabLabels[kTabCount] = {STR_DETAIL_TAB_INFO, STR_DETAIL_TAB_STATS, STR_MEMBER_TAB_MOVES};
const StrId kStatLabels[6] = {STR_STAT_HP, STR_STAT_ATK, STR_STAT_DEF, STR_STAT_SPA, STR_STAT_SPD, STR_STAT_SPE};

void on_back(lv_event_t *) {
  SavesState &st = saves_state();
  st.tab = st.from == MemberFrom::Box ? SaveTab::Boxes : SaveTab::Party;
  if (st.from == MemberFrom::Box) st.box_slot = st.member;
  nav_go(Screen::Save);
}

// Vizinho ocupado de `from` andando `dir`; 0xFF se não houver.
uint8_t neighbor(int8_t dir) {
  uint8_t limit = saves_state().from == MemberFrom::Box ? SAVE_BOX_SLOTS : s_count;
  for (int i = (int)saves_state().member + dir; i >= 0 && i < limit; i += dir) {
    if (s_list[i].species) return (uint8_t)i;
  }
  return 0xFF;
}

void on_neighbor(lv_event_t *e) {
  saves_state().member = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  nav_go(Screen::Member);
}

void nav_button(lv_obj_t *h, const lv_img_dsc_t *icon, uint8_t target) {
  lv_obj_t *b = ui_header_button(h, icon, on_neighbor, (void *)(uintptr_t)target);
  if (target == 0xFF) {
    lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_recolor(lv_obj_get_child(b, 0), c(theme::FAINT_2), 0);
  }
}

void on_species(lv_event_t *) { open_species_from(s_mon.species, Screen::Member); }

// ---------- Info ----------

void info_row(lv_obj_t *dl, uint8_t i, StrId label, const char *value, bool divider) {
  lv_obj_t *row = ui_box(dl);
  lv_obj_set_pos(row, 0, i * 24);
  lv_obj_set_size(row, LV_PCT(100), 24);
  if (divider) {
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, c(theme::ROW_DIVIDER), 0);
  }
  lv_obj_align(ui_label(row, &font_12_500, theme::MUTED, tr(label)), LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t *v = ui_label(row, &font_14_600, theme::TEXT, value);
  lv_obj_set_style_max_width(v, 120, 0);
  lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
  lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
}

void build_info(lv_obj_t *p) {
  lv_obj_t *frame = ui_box(p);
  lv_obj_set_pos(frame, 8, 8);
  lv_obj_set_size(frame, 112, 112);
  lv_obj_set_style_radius(frame, 10, 0);
  lv_obj_set_style_bg_color(frame, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
  lv_obj_center(ui_mon_sprite(frame, s_mon.egg ? 0 : s_mon.species, SpriteSize::S96, &s_sprite, !s_mon.egg));

  lv_obj_t *pill = ui_box(p);
  lv_obj_set_pos(pill, 8, 8 + 112 + 6);
  lv_obj_set_size(pill, 112, 24);
  lv_obj_set_style_radius(pill, 12, 0);
  lv_obj_t *row = ui_box(pill);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (s_mon.shiny) {
    lv_obj_set_style_bg_color(pill, c(0x2A2614), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    ui_icon(row, &icon_star_13, theme::NOTICE_IDLE);
    ui_label(row, &font_12_700, theme::NOTICE_IDLE, tr(STR_MEMBER_SHINY));
  } else {
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, c(theme::LINE), 0);
    ui_label(row, &font_12_500, theme::MUTED, tr(STR_SAVE_LEVEL));
    lv_obj_t *lv = ui_label(row, &font_14_700, theme::TEXT, "");
    if (s_mon.level) lv_label_set_text_fmt(lv, "%u", s_mon.level);
    else lv_label_set_text(lv, "?");
  }
  lv_obj_center(row);

  constexpr lv_coord_t kX = 8 + 112 + 10, kW = 320 - kX - 8;
  lv_obj_t *name = ui_label(p, &font_20_700, theme::TEXT, s_mon.egg ? tr(STR_MEMBER_EGG) : s_mon.nick);
  lv_obj_set_pos(name, kX, 8);
  lv_obj_set_width(name, kW);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

  lv_obj_t *link = ui_box(p);
  lv_obj_set_pos(link, kX, 34);
  lv_obj_set_size(link, LV_SIZE_CONTENT, 16);
  lv_obj_set_style_pad_column(link, 2, 0);
  lv_obj_set_flex_flow(link, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(link, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  char buf[48];
  snprintf(buf, sizeof(buf), tr(STR_MEMBER_SPECIES_FMT), species_repo_name(s_mon.species), s_mon.species);
  ui_label(link, &font_12_600, theme::MUTED, buf);
  ui_icon(link, &icon_forward_12, theme::MUTED);
  lv_obj_set_ext_click_area(link, 8);
  ui_on_click(link, on_species);

  lv_obj_t *chips = ui_box(p);
  lv_obj_set_pos(chips, kX, 54);
  lv_obj_set_size(chips, kW, 20);
  lv_obj_set_style_pad_column(chips, 4, 0);
  lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
  SpeciesType t[2];
  species_repo_types(s_mon.species, &t[0], &t[1]);
  for (SpeciesType ty : t) {
    if (ty != SpeciesType::None) ui_chip(chips, species_type_name(ty), species_type_color(ty), theme::ON_ACCENT);
  }

  lv_obj_t *dl = ui_box(p);
  lv_obj_set_pos(dl, kX, 80);
  lv_obj_set_size(dl, kW, 3 * 24);
  char item[NAME_TABLE_NAME_SIZE];
  if (!s_mon.item) strcpy(item, tr(STR_MEMBER_NONE));
  else if (!item_name(s_head.game, s_mon.item, item, sizeof(item))) snprintf(item, sizeof(item), "#%u", s_mon.item);
  snprintf(buf, sizeof(buf), tr(STR_MEMBER_OT_FMT), s_mon.ot, s_mon.tid);
  info_row(dl, 0, STR_MEMBER_NATURE, nature_name(s_mon.nature), true);
  info_row(dl, 1, STR_MEMBER_ITEM, item, true);
  info_row(dl, 2, STR_MEMBER_OT, buf, false);
}

// ---------- Status ----------

void build_stats(lv_obj_t *p) {
  constexpr lv_coord_t kPadX = 10;
  uint8_t up, down;
  nature_effect(s_mon.nature, &up, &down);
  bool neutral = up == down;

  lv_obj_t *head = ui_box(p);
  lv_obj_set_pos(head, kPadX, 8);
  lv_obj_set_size(head, 320 - 2 * kPadX, 18);
  lv_obj_set_style_pad_column(head, 4, 0);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  char buf[48], lv[12];
  if (s_mon.level) snprintf(lv, sizeof(lv), tr(STR_EVO_LEVEL_FMT), s_mon.level);
  else snprintf(lv, sizeof(lv), "%s ?", tr(STR_SAVE_LEVEL));
  snprintf(buf, sizeof(buf), "%s · %s", s_mon.egg ? tr(STR_MEMBER_EGG) : s_mon.nick, lv);
  ui_label(head, &font_12_600, theme::MUTED, buf);
  ui_header_spacer(head);
  if (!neutral) {
    ui_icon(head, &icon_up_12, theme::ACCENT);
    ui_label(head, &font_12_600, theme::ACCENT, tr(kStatLabels[up]));
    ui_icon(head, &icon_down_12, kDown);
    ui_label(head, &font_12_600, kDown, tr(kStatLabels[down]));
  }
  lv_obj_t *nat = ui_label(head, &font_12_500, theme::MUTED, nature_name(s_mon.nature));
  lv_obj_set_style_pad_left(nat, 4, 0);

  constexpr lv_coord_t kTop = 32, kStep = 22, kLabelW = 34, kValueW = 58, kGap = 6;
  constexpr lv_coord_t kBarX = kPadX + kLabelW + kGap + kValueW + kGap, kBarW = 320 - kPadX - kBarX;
  for (uint8_t i = 0; i < 6; i++) {
    lv_coord_t y = kTop + i * kStep;
    uint32_t color = (!neutral && i == up) ? theme::ACCENT : (!neutral && i == down) ? kDown : theme::TEXT;
    lv_obj_set_pos(ui_label(p, &font_12_600, theme::MUTED, tr(kStatLabels[i])), kPadX, y + 3);
    lv_obj_t *v = ui_label(p, &font_14_700, theme::TEXT, "");
    if (!s_mon.level) lv_label_set_text(v, "?");
    else if (i == 0) lv_label_set_text_fmt(v, "%u/%u", s_mon.hp, s_mon.stats[0]);
    else lv_label_set_text_fmt(v, "%u", s_mon.stats[i]);
    lv_obj_set_width(v, kValueW);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(v, kPadX + kLabelW + kGap, y + 2);
    lv_obj_t *bar = ui_progress(p, kBarW, 6);
    lv_obj_set_pos(bar, kBarX, y + 8);
    lv_obj_set_style_bg_color(bar, c(color), LV_PART_INDICATOR);
    uint16_t val = s_mon.stats[i];
    ui_progress_set(bar, val > 250 ? 250 : val, 250);  // escala: status reais até ~250 no Nv 50–60
  }
}

// ---------- Golpes ----------

void build_moves(lv_obj_t *p) {
  constexpr lv_coord_t kRowH = 34, kGap = 4, kTop = (kBodyH - (4 * kRowH + 3 * kGap)) / 2;
  for (uint8_t i = 0; i < 4; i++) {
    lv_obj_t *row = ui_box(p);
    lv_obj_set_pos(row, 8, kTop + i * (kRowH + kGap));
    lv_obj_set_size(row, 320 - 16, kRowH);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_bg_color(row, c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(row, 6, 0);
    lv_obj_set_style_pad_right(row, 10, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    uint16_t id = s_mon.moves[i];
    if (!id) {
      ui_label(row, &font_14_600, theme::FAINT, "—");
      continue;
    }
    MoveInfo mi;
    bool known = move_info(id, &mi);
    SpeciesType ty = known ? (SpeciesType)mi.type : SpeciesType::Normal;
    lv_obj_t *chip = ui_chip(row, species_type_abbr(ty), species_type_color(ty), theme::ON_ACCENT, 0);
    lv_obj_set_width(chip, 40);
    lv_obj_center(lv_obj_get_child(chip, 0));
    char name[NAME_TABLE_NAME_SIZE];
    if (known) strcpy(name, mi.name);
    else snprintf(name, sizeof(name), "#%u", id);
    lv_obj_t *n = ui_label(row, &font_14_600, theme::TEXT, name);
    lv_obj_set_flex_grow(n, 1);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    ui_label(row, &font_12_500, theme::MUTED, tr(STR_MEMBER_PP));
    lv_obj_t *pp = ui_label(row, &font_12_700, theme::TEXT, "");
    if (known) lv_label_set_text_fmt(pp, "%u/%u", s_mon.pp[i], move_pp_max(mi.pp, s_mon.pp_ups[i]));
    else lv_label_set_text_fmt(pp, "%u", s_mon.pp[i]);
    lv_obj_set_width(pp, 40);
    lv_obj_set_style_text_align(pp, LV_TEXT_ALIGN_RIGHT, 0);
  }
}

// ---------- abas ----------

void show_tab(uint8_t tab) {
  s_tab = tab;
  for (uint8_t i = 0; i < kTabCount; i++) {
    bool on = i == tab;
    if (on) lv_obj_clear_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *label = lv_obj_get_child(s_tabs[i], 0);
    lv_obj_t *mark = lv_obj_get_child(s_tabs[i], 1);
    lv_obj_set_style_text_font(label, on ? &font_14_700 : &font_14_600, 0);
    lv_obj_set_style_text_color(label, c(on ? theme::ACCENT : theme::TEXT), 0);
    if (on) lv_obj_clear_flag(mark, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
  }
}

void on_tab(lv_event_t *e) {
  uint8_t t = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (t != s_tab) show_tab(t);
}

void build_tabs(lv_obj_t *scr) {
  lv_obj_t *bar = ui_box(scr);
  lv_obj_set_pos(bar, 0, 240 - theme::TABS_H);
  lv_obj_set_size(bar, 320, theme::TABS_H);
  lv_obj_set_style_bg_color(bar, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_color(bar, c(theme::LINE), 0);
  for (uint8_t i = 0; i < kTabCount; i++) {
    lv_coord_t x0 = i * 320 / kTabCount, x1 = (i + 1) * 320 / kTabCount;
    lv_obj_t *t = ui_box(bar);
    lv_obj_set_pos(t, x0, 0);
    lv_obj_set_size(t, x1 - x0, theme::TABS_H - 1);
    lv_obj_center(ui_label(t, &font_14_600, theme::TEXT, tr(kTabLabels[i])));
    lv_obj_t *mark = ui_box(t);
    lv_obj_set_size(mark, LV_PCT(100), 3);
    lv_obj_set_style_bg_color(mark, c(theme::ACCENT), 0);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
    ui_on_click(t, on_tab, (void *)(uintptr_t)i);
    s_tabs[i] = t;
  }
}

void on_delete(lv_event_t *) {
  if (s_m) sprite_free(&s_sprite);
  saves_screen_mem_release();
  s_m = nullptr;
}

bool load_mon() {
  SavesState &st = saves_state();
  if (!save_store_head(st.save, &s_head) || !save_store_open(st.save)) return false;
  memset(s_list, 0, sizeof(s_list));
  if (st.from == MemberFrom::Box) {
    if (!save_store_box(st.box, s_list, s_box_name, sizeof(s_box_name))) return false;
    s_count = SAVE_BOX_SLOTS;
  } else {
    s_count = save_store_party(s_list);
  }
  if (st.member >= SAVE_BOX_SLOTS || !s_list[st.member].species) return false;
  s_mon = s_list[st.member];
  return true;
}

}  // namespace

lv_obj_t *member_screen_create() {
  s_m = (Mem *)saves_screen_mem_take(sizeof(Mem));
  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);
  if (!s_m || !load_mon()) {
    ui_header(scr, tr(STR_TITLE_SAVES), on_back);
    return scr;
  }
  SavesState &st = saves_state();
  lv_obj_t *h = ui_header(scr, nullptr, on_back);
  char where[48];
  if (st.from == MemberFrom::Box) snprintf(where, sizeof(where), tr(STR_MEMBER_BOX_FMT), s_head.name, s_box_name);
  else snprintf(where, sizeof(where), tr(STR_MEMBER_PARTY_FMT), s_head.name, st.member + 1, s_count);
  ui_label(h, &font_12_600, theme::MUTED, where);
  ui_header_spacer(h);
  nav_button(h, &icon_prev_18, neighbor(-1));
  nav_button(h, &icon_forward_18, neighbor(1));

  for (uint8_t i = 0; i < kTabCount; i++) {
    lv_obj_t *p = ui_box(scr);
    lv_obj_set_pos(p, 0, kBodyY);
    lv_obj_set_size(p, 320, kBodyH);
    s_panels[i] = p;
  }
  build_info(s_panels[0]);  // o 96 primeiro: é o maior bloco
  build_stats(s_panels[1]);
  build_moves(s_panels[2]);
  build_tabs(scr);
  show_tab(0);
  return scr;
}
