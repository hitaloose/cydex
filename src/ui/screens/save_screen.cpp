// 17 Save: Resumo, 17a Time, 17b Caixas (canvas: SaveResumo, SaveTime,
// SaveCaixas). Uma tela só; cada aba é um painel (como a Ficha).
//   cabeçalho: voltar (Saves), treinador, chip do jogo; ‹ › trocam de save
//   (na aba Caixas, no lugar deles, "N nas caixas")
//   Resumo: cartão do treinador, insígnias, dex deste save, dinheiro,
//   Hall da Fama (toque abre o 19 filtrado) e brilhantes. Só o saves.cache.
//   Time: 3x2 com sprite 48, nível, ★ e barra de PS. Toque abre o Membro.
//   Caixas: 6x5 com ícone 24 + painel do selecionado (sprite 48). ‹ › do
//   painel trocam de caixa. Toque no painel abre o Membro.
// Memória: time e caixa são lidos do save quando a aba abre. Os sprites da
// aba que sai são liberados antes de carregar os da que entra (Time 6x48 =
// 24 vagas; Caixas 30x24 + 48 = 34 de 40).
#include <cstdio>
#include <cstring>

#include "data/SpeciesRepo.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/saves_ui.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kBodyY = theme::HEADER_H;
constexpr lv_coord_t kBodyH = 240 - theme::HEADER_H - theme::TABS_H;  // 170
constexpr uint8_t kTabCount = 3;
constexpr lv_coord_t kCell = 28, kCellGap = 2;

// Dados grandes na área compartilhada das telas de saves (saves_ui.h).
struct Mem {
  SaveEntry entry;
  SaveMon party[6];
  Sprite party_spr[6];
  SaveMon box[SAVE_BOX_SLOTS];
  Sprite box_spr[SAVE_BOX_SLOTS];
  Sprite sel_spr;
};
static_assert(sizeof(Mem) <= SAVES_SCREEN_MEM, "Save não cabe na área das telas");
Mem *s_m;
#define s_entry (s_m->entry)
#define s_party (s_m->party)
#define s_party_spr (s_m->party_spr)
#define s_box (s_m->box)
#define s_box_spr (s_m->box_spr)
#define s_sel_spr (s_m->sel_spr)

lv_obj_t *s_panels[kTabCount];
lv_obj_t *s_tabs[kTabCount];
lv_obj_t *s_nav[2];
lv_obj_t *s_total;
bool s_built[kTabCount];

uint8_t s_party_n;
char s_box_name[24];
lv_obj_t *s_cells[SAVE_BOX_SLOTS];
lv_obj_t *s_side;  // painel do selecionado

const StrId kTabLabels[kTabCount] = {STR_SAVE_TAB_SUMMARY, STR_SAVE_TAB_PARTY, STR_SAVE_TAB_BOXES};

void free_party_sprites() {
  for (Sprite &s : s_party_spr) sprite_free(&s);
}
void free_box_sprites() {
  for (Sprite &s : s_box_spr) sprite_free(&s);
  sprite_free(&s_sel_spr);
}

// ---------- cabeçalho ----------

void on_back(lv_event_t *) { nav_go(Screen::Saves); }

// Save reconhecido vizinho de `from` andando `dir`; 0xFF se não houver.
uint8_t neighbor(uint8_t from, int8_t dir) {
  SaveHead h;
  for (int i = (int)from + dir; i >= 0 && i < save_store_count(); i += dir) {
    if (save_store_head((uint8_t)i, &h)) return (uint8_t)i;
  }
  return 0xFF;
}

void on_neighbor(lv_event_t *e) {
  SavesState &st = saves_state();
  st.save = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  st.box = 0;
  st.box_slot = 0xFF;
  nav_go(Screen::Save);
}

void nav_button(lv_obj_t *h, uint8_t i, const lv_img_dsc_t *icon, uint8_t target) {
  lv_obj_t *b = ui_header_button(h, icon, on_neighbor, (void *)(uintptr_t)target);
  if (target == 0xFF) {
    lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_recolor(lv_obj_get_child(b, 0), c(theme::FAINT_2), 0);
  }
  s_nav[i] = b;
}

// ---------- Resumo ----------

lv_obj_t *tile(lv_obj_t *p, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
  lv_obj_t *t = ui_box(p);
  lv_obj_set_pos(t, x, y);
  lv_obj_set_size(t, w, h);
  lv_obj_set_style_radius(t, 8, 0);
  lv_obj_set_style_bg_color(t, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  return t;
}

void on_hof(lv_event_t *) {
  SavesState &st = saves_state();
  st.hof_filter = st.save;
  st.hof_page = 0;
  nav_go(Screen::HallOfFame);
}

void build_summary(lv_obj_t *p) {
  const SaveSummary &s = s_entry.summary;
  uint32_t game = save_game_color(s.game);
  constexpr lv_coord_t kX = 8, kW = 320 - 16;

  // Avatar: inicial do nome na cor do jogo.
  lv_obj_t *av = ui_box(p);
  lv_obj_set_pos(av, kX, 7);
  lv_obj_set_size(av, 40, 40);
  lv_obj_set_style_radius(av, 10, 0);
  lv_obj_set_style_border_width(av, 1, 0);
  lv_obj_set_style_border_color(av, c(game), 0);
  lv_obj_set_style_bg_color(av, lv_color_mix(c(game), c(theme::BG), 48), 0);
  lv_obj_set_style_bg_opa(av, LV_OPA_COVER, 0);
  char initial[5] = {0};
  size_t len = (s.name[0] & 0x80) ? 2 : 1;  // Latin-1 em UTF-8: até 2 bytes
  memcpy(initial, s.name, len);
  lv_obj_center(ui_label(av, &font_20_700, game, initial));

  lv_obj_t *row = ui_box(p);
  lv_obj_set_pos(row, kX + 50, 8);
  lv_obj_set_size(row, 160, 20);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_label(row, &font_16_700, theme::TEXT, s.name);
  ui_gender(row, s.gender, 13);
  char buf[64];
  snprintf(buf, sizeof(buf), tr(STR_SAVE_ID_FMT), s.tid, s_entry.file);
  lv_obj_t *id = ui_label(p, &font_12_500, theme::MUTED, buf);
  ui_label_clamp(id, 150, 1);  // nome no-intro é longo: uma linha com "…"
  lv_obj_set_pos(id, kX + 50, 30);

  lv_obj_t *tl = ui_label(p, &font_12_500, theme::MUTED, tr(STR_SAVE_TIME));
  lv_obj_align(tl, LV_ALIGN_TOP_RIGHT, -kX, 10);
  format_playtime(s.hours, s.minutes, buf, sizeof(buf));
  lv_obj_t *tv = ui_label(p, &font_14_700, theme::TEXT, buf);
  lv_obj_align(tv, LV_ALIGN_TOP_RIGHT, -kX, 26);

  // Insígnias
  lv_obj_t *b = tile(p, kX, 52, kW, 28);
  lv_obj_align(ui_label(b, &font_12_600, theme::MUTED, tr(STR_SAVE_BADGES)), LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_t *count = ui_label(b, &font_12_700, theme::TEXT, "");
  lv_label_set_text_fmt(count, "%u/8", s.badge_count());
  lv_obj_align(count, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_t *icons = ui_badges(b, s.badges, 10, 6);
  lv_obj_align(icons, LV_ALIGN_RIGHT_MID, -42, 0);

  // Dex deste save: visto (faint) por baixo do capturado (accent).
  lv_obj_t *d = tile(p, kX, 85, kW, 36);
  lv_obj_set_pos(ui_label(d, &font_12_600, theme::MUTED, tr(STR_SAVE_DEX)), 10, 4);
  snprintf(buf, sizeof(buf), tr(STR_SAVE_DEX_FMT), s.seen, s.dex_total);
  lv_obj_t *rest = ui_label(d, &font_12_500, theme::MUTED, buf);
  lv_obj_align(rest, LV_ALIGN_TOP_RIGHT, -10, 4);
  lv_obj_t *caught = ui_label(d, &font_12_700, theme::TEXT, "");
  lv_label_set_text_fmt(caught, "%u", s.caught);
  lv_obj_align_to(caught, rest, LV_ALIGN_OUT_LEFT_MID, -5, 0);
  lv_obj_t *seen = ui_progress(d, kW - 20, 5);
  lv_obj_set_pos(seen, 10, 24);
  lv_obj_set_style_bg_color(seen, c(theme::FAINT), LV_PART_INDICATOR);
  ui_progress_set(seen, s.seen, s.dex_total);
  lv_obj_t *cb = ui_progress(d, kW - 20, 5);
  lv_obj_set_pos(cb, 10, 24);
  lv_obj_set_style_bg_opa(cb, LV_OPA_TRANSP, 0);
  ui_progress_set(cb, s.caught, s.dex_total);

  // Dinheiro, Hall da Fama, Brilhantes
  constexpr lv_coord_t kTileW = (kW - 10) / 3, kTileY = 126, kTileH = 37;
  StrId labels[3] = {STR_SAVE_MONEY, STR_TITLE_HOF, STR_SAVE_SHINY};
  for (int i = 0; i < 3; i++) {
    lv_obj_t *t = tile(p, kX + i * (kTileW + 5), kTileY, kTileW, kTileH);
    lv_obj_set_pos(ui_label(t, &font_12_500, theme::MUTED, tr(labels[i])), 8, 3);
    lv_obj_t *v = ui_box(t);
    lv_obj_set_pos(v, 8, 18);
    lv_obj_set_size(v, kTileW - 16, 17);
    lv_obj_set_style_pad_column(v, 4, 0);
    lv_obj_set_flex_flow(v, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(v, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    char num[24], text[32];
    if (i == 0) {
      format_thousands(s.money, num, sizeof(num));
      snprintf(text, sizeof(text), tr(STR_SAVE_MONEY_FMT), num);
    } else if (i == 1) {
      if (s.hof_count == 1) snprintf(text, sizeof(text), "%s", tr(STR_SAVE_HOF_ONE));
      else snprintf(text, sizeof(text), tr(STR_SAVE_HOF_FMT), s.hof_count);
      if (s.hof_count) {
        lv_obj_set_style_bg_color(t, c(theme::SURFACE_2), LV_STATE_PRESSED);
        ui_on_click(t, on_hof);
      }
    } else {
      ui_icon(v, &icon_star_12, theme::NOTICE_IDLE);
      snprintf(text, sizeof(text), "%u", s.shiny_count);
    }
    ui_label(v, &font_14_700, theme::TEXT, text);
  }
}

// ---------- Time ----------

void on_member(lv_event_t *e) {
  SavesState &st = saves_state();
  uintptr_t v = (uintptr_t)lv_event_get_user_data(e);
  st.from = (MemberFrom)(v >> 8);
  st.member = (uint8_t)(v & 0xFF);
  nav_go(Screen::Member);
}

void build_party(lv_obj_t *p) {
  s_party_n = save_store_open(saves_state().save) ? save_store_party(s_party) : 0;
  constexpr lv_coord_t kPad = 6, kGap = 4;
  constexpr lv_coord_t kW = (320 - 2 * kPad - 2 * kGap) / 3, kH = (kBodyH - 2 * kPad - kGap) / 2;
  for (uint8_t i = 0; i < 6; i++) {
    lv_coord_t x = kPad + (i % 3) * (kW + kGap), y = kPad + (i / 3) * (kH + kGap);
    if (i >= s_party_n) {
      ui_mon_empty(p, x, y, kW, kH, true);
      continue;
    }
    const SaveMon &m = s_party[i];
    MonCardSpec spec{m.species, m.nick, m.level, m.shiny, m.egg, m.hp, m.stats[0], nullptr};
    ui_mon_card(p, x, y, kW, kH, spec, &s_party_spr[i], on_member,
                (void *)(uintptr_t)(((uintptr_t)MemberFrom::Party << 8) | i));
  }
}

// ---------- Caixas ----------

void render_side();

void style_cell(uint8_t i) {
  lv_obj_t *cell = s_cells[i];
  bool on = i == saves_state().box_slot;
  bool filled = s_box[i].species != 0;
  lv_obj_set_style_bg_opa(cell, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cell, on ? 2 : 1, 0);
  lv_obj_set_style_border_color(cell, c(on ? theme::ACCENT : (filled ? theme::SURFACE : theme::ROW_DIVIDER)), 0);
}

void on_cell(lv_event_t *e) {
  SavesState &st = saves_state();
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (i == st.box_slot) return;
  uint8_t old = st.box_slot;
  st.box_slot = i;
  if (old < SAVE_BOX_SLOTS) style_cell(old);
  style_cell(i);
  render_side();
}

void on_open_selected(lv_event_t *) {
  SavesState &st = saves_state();
  if (st.box_slot >= SAVE_BOX_SLOTS || !s_box[st.box_slot].species) return;
  st.from = MemberFrom::Box;
  st.member = st.box_slot;
  nav_go(Screen::Member);
}

void on_box(lv_event_t *e) {
  SavesState &st = saves_state();
  int8_t dir = (int8_t)(intptr_t)lv_event_get_user_data(e);
  int next = (int)st.box + dir;
  if (next < 0 || next >= s_entry.summary.box_total) return;
  st.box = (uint8_t)next;
  st.box_slot = 0xFF;
  // Remonta a tela (o botão tocado fica dentro do painel que seria limpo).
  nav_go(Screen::Save);
}

lv_obj_t *box_arrow(lv_obj_t *parent, const lv_img_dsc_t *icon, int8_t dir, bool enabled) {
  lv_obj_t *b = ui_box(parent);
  lv_obj_set_size(b, 32, 32);
  ui_icon(b, icon, enabled ? theme::TEXT : theme::FAINT_2);
  lv_obj_center(lv_obj_get_child(b, 0));
  if (enabled) ui_on_click(b, on_box, (void *)(intptr_t)dir);
  return b;
}

void render_side() {
  lv_obj_clean(s_side);
  sprite_free(&s_sel_spr);
  SavesState &st = saves_state();
  uint8_t count = 0;
  for (const SaveMon &m : s_box) count += m.species ? 1 : 0;

  lv_obj_t *nav = ui_box(s_side);
  lv_obj_set_size(nav, LV_PCT(100), 32);
  lv_obj_set_style_radius(nav, 8, 0);
  lv_obj_set_style_bg_color(nav, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
  lv_obj_t *prev = box_arrow(nav, &icon_prev_18, -1, st.box > 0);
  lv_obj_align(prev, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t *next = box_arrow(nav, &icon_forward_18, 1, st.box + 1 < s_entry.summary.box_total);
  lv_obj_align(next, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_t *name = ui_label(nav, &font_12_700, theme::TEXT, s_box_name);
  lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 2);
  lv_obj_t *sub = ui_label(nav, &font_12_500, theme::MUTED, "");
  lv_label_set_text_fmt(sub, tr(STR_BOX_COUNT_FMT), count, st.box + 1, s_entry.summary.box_total);
  lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -1);

  lv_obj_t *card = ui_box(s_side);
  lv_obj_set_pos(card, 0, 36);
  lv_obj_set_size(card, LV_PCT(100), 148 - 36);
  lv_obj_set_style_radius(card, 8, 0);
  if (st.box_slot >= SAVE_BOX_SLOTS) {
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, c(theme::FAINT_2), 0);
    lv_obj_center(ui_label(card, &font_12_500, theme::MUTED, tr(STR_BOX_EMPTY)));
    return;
  }
  const SaveMon &m = s_box[st.box_slot];
  lv_obj_set_style_bg_color(card, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, c(theme::LINE), 0);
  lv_obj_set_style_bg_color(card, c(theme::SURFACE_2), LV_STATE_PRESSED);
  ui_on_click(card, on_open_selected);

  lv_obj_t *img = ui_mon_sprite(card, m.egg ? 0 : m.species, SpriteSize::S48, &s_sel_spr, !m.egg);
  lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 6);
  if (m.shiny) lv_obj_align_to(ui_icon(card, &icon_star_12, theme::NOTICE_IDLE), img, LV_ALIGN_OUT_RIGHT_TOP, -4, -4);
  lv_obj_t *nick = ui_label(card, &font_14_700, theme::TEXT, m.egg ? tr(STR_MEMBER_EGG) : m.nick);
  lv_obj_set_width(nick, 108);
  lv_obj_set_style_text_align(nick, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(nick, LV_LABEL_LONG_DOT);
  lv_obj_align(nick, LV_ALIGN_TOP_MID, 0, 57);
  char lv[12], buf[32];
  if (m.level) snprintf(lv, sizeof(lv), tr(STR_EVO_LEVEL_FMT), m.level);
  else snprintf(lv, sizeof(lv), "%s ?", tr(STR_SAVE_LEVEL));
  snprintf(buf, sizeof(buf), tr(STR_BOX_SUB_FMT), lv, m.species);
  lv_obj_align(ui_label(card, &font_12_500, theme::MUTED, buf), LV_ALIGN_TOP_MID, 0, 74);
  lv_obj_t *open = ui_box(card);
  lv_obj_set_size(open, LV_SIZE_CONTENT, 14);
  lv_obj_set_flex_flow(open, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(open, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(open, 2, 0);
  ui_label(open, &font_12_700, theme::ACCENT, tr(STR_BOX_OPEN));
  ui_icon(open, &icon_forward_12, theme::ACCENT);
  lv_obj_align(open, LV_ALIGN_BOTTOM_MID, 0, -5);
}

void build_boxes(lv_obj_t *p) {
  free_box_sprites();
  SavesState &st = saves_state();
  bool ok = save_store_open(st.save) && save_store_box(st.box, s_box, s_box_name, sizeof(s_box_name));
  if (!ok) memset(s_box, 0, sizeof(s_box));
  if (st.focus_species) {  // veio da Ficha › Onde está
    for (uint8_t i = 0; i < SAVE_BOX_SLOTS; i++) {
      if (s_box[i].species == st.focus_species) {
        st.box_slot = i;
        break;
      }
    }
    st.focus_species = 0;
  }
  if (st.box_slot >= SAVE_BOX_SLOTS || !s_box[st.box_slot].species) {
    st.box_slot = 0xFF;
    for (uint8_t i = 0; i < SAVE_BOX_SLOTS; i++) {
      if (s_box[i].species) {
        st.box_slot = i;
        break;
      }
    }
  }

  constexpr lv_coord_t kGridW = 6 * kCell + 5 * kCellGap, kGridH = 5 * kCell + 4 * kCellGap;
  constexpr lv_coord_t kGridX = 6, kGridY = (kBodyH - kGridH) / 2;
  for (uint8_t i = 0; i < SAVE_BOX_SLOTS; i++) {
    lv_obj_t *cell = ui_box(p);
    lv_obj_set_pos(cell, kGridX + (i % 6) * (kCell + kCellGap), kGridY + (i / 6) * (kCell + kCellGap));
    lv_obj_set_size(cell, kCell, kCell);
    lv_obj_set_style_radius(cell, 4, 0);
    lv_obj_set_style_bg_color(cell, c(theme::SURFACE), 0);
    s_cells[i] = cell;
    style_cell(i);
    const SaveMon &m = s_box[i];
    if (!m.species) continue;
    lv_obj_t *img = ui_mon_sprite(cell, m.egg ? 0 : m.species, SpriteSize::S24, &s_box_spr[i], !m.egg);
    lv_obj_center(img);
    if (m.shiny) lv_obj_align(ui_icon(cell, &icon_star_9, theme::NOTICE_IDLE), LV_ALIGN_TOP_RIGHT, 1, -1);
    lv_obj_set_ext_click_area(cell, 1);  // cobre o vão de 2 px
    ui_on_click(cell, on_cell, (void *)(uintptr_t)i);
  }

  s_side = ui_box(p);
  lv_obj_set_pos(s_side, kGridX + kGridW + 6, kGridY);
  lv_obj_set_size(s_side, 320 - (kGridX + kGridW + 6) - 6, kGridH);
  render_side();
}

// ---------- abas ----------

void show_tab(SaveTab tab) {
  SavesState &st = saves_state();
  st.tab = tab;
  uint8_t t = (uint8_t)tab;
  // Sprites só da aba visível (a área de sprites não cabe Time + Caixas).
  if (tab != SaveTab::Party && s_built[1]) {
    free_party_sprites();
    lv_obj_clean(s_panels[1]);
    s_built[1] = false;
  }
  if (tab != SaveTab::Boxes && s_built[2]) {
    free_box_sprites();
    lv_obj_clean(s_panels[2]);
    s_built[2] = false;
  }
  if (!s_built[t]) {
    s_built[t] = true;
    if (tab == SaveTab::Summary) build_summary(s_panels[0]);
    else if (tab == SaveTab::Party) build_party(s_panels[1]);
    else build_boxes(s_panels[2]);
  }
  for (uint8_t i = 0; i < kTabCount; i++) {
    bool on = i == t;
    if (on) lv_obj_clear_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *label = lv_obj_get_child(s_tabs[i], 0);
    lv_obj_t *mark = lv_obj_get_child(s_tabs[i], 1);
    lv_obj_set_style_text_font(label, on ? &font_14_700 : &font_14_600, 0);
    lv_obj_set_style_text_color(label, c(on ? theme::ACCENT : theme::TEXT), 0);
    if (on) lv_obj_clear_flag(mark, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
  }
  bool boxes = tab == SaveTab::Boxes;
  for (lv_obj_t *b : s_nav) {
    if (boxes) lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(b, LV_OBJ_FLAG_HIDDEN);
  }
  if (boxes) lv_obj_clear_flag(s_total, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_total, LV_OBJ_FLAG_HIDDEN);
}

void on_tab(lv_event_t *e) {
  SaveTab tab = (SaveTab)(uintptr_t)lv_event_get_user_data(e);
  if (tab != saves_state().tab) show_tab(tab);
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
  if (s_m) {
    free_party_sprites();
    free_box_sprites();
  }
  saves_screen_mem_release();
  s_m = nullptr;
}

}  // namespace

lv_obj_t *save_screen_create() {
  SavesState &st = saves_state();
  for (bool &b : s_built) b = false;
  s_m = (Mem *)saves_screen_mem_take(sizeof(Mem));
  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);
  if (!s_m || !save_store_entry(st.save, &s_entry) || !s_entry.ok) {
    ui_header(scr, tr(STR_TITLE_SAVES), on_back);
    return scr;
  }

  lv_obj_t *h = ui_header(scr, s_entry.summary.name, on_back);
  lv_obj_t *chip = ui_game_chip(h, s_entry.summary.game);
  lv_obj_set_style_pad_left(chip, 7, 0);
  lv_obj_set_style_translate_x(chip, 6, 0);
  ui_header_spacer(h);
  nav_button(h, 0, &icon_prev_18, neighbor(st.save, -1));
  nav_button(h, 1, &icon_forward_18, neighbor(st.save, 1));
  s_total = ui_label(h, &font_12_600, theme::MUTED, "");
  lv_label_set_text_fmt(s_total, tr(STR_BOX_TOTAL_FMT), s_entry.summary.box_count);
  lv_obj_set_style_pad_right(s_total, 10, 0);

  for (uint8_t i = 0; i < kTabCount; i++) {
    lv_obj_t *p = ui_box(scr);
    lv_obj_set_pos(p, 0, kBodyY);
    lv_obj_set_size(p, 320, kBodyH);
    s_panels[i] = p;
  }
  build_tabs(scr);
  show_tab(st.tab);
  return scr;
}
