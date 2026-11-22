// 10–13 Ficha (canvas: Detalhe, DetalheStatus, DetalheEvol, NaoVisto).
//   cabeçalho: voltar, #nº (+ nome nas abas Status/Evolução), ‹ ›
//   corpo: um painel por aba; trocar de aba só esconde/mostra (sem reler SD)
//   abas 38 px: Info / Status / Evolução (as duas últimas apagadas em não visto)
// Regras que o canvas não mostra estão na nota "Ficha: regras" do canvas.
//
// Memória: o registro é lido uma vez ao abrir. O sprite 96 (18 KB, o maior
// bloco) é alocado primeiro; os 64 da Evolução só na 1a vez que a aba abre.
// Tudo é liberado no LV_EVENT_DELETE.
#include <cstdio>

#include "data/SaveStore.h"
#include "data/SpeciesRepo.h"
#include "data/SpriteLoader.h"
#include "data/StatusStore.h"
#include "dex/Dex.h"
#include "i18n/i18n.h"
#include "logic/Format.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/icons/icons.h"
#include "ui/saves_ui.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "util/log.h"

namespace {

constexpr lv_coord_t kBodyY = theme::HEADER_H;
constexpr lv_coord_t kBodyH = 240 - theme::HEADER_H - theme::TABS_H;  // 170
constexpr lv_coord_t kTabsY = 240 - theme::TABS_H;
constexpr uint8_t kTabCount = 3;

// Evolução: card 76 + coluna da seta 36, gap 2 (canvas).
constexpr lv_coord_t kStageW = 76, kArrowW = 36, kEvoGap = 2;

uint16_t s_n;
bool s_unknown;
bool s_have_rec;
SpeciesRecord s_rec;
Sprite s_sprite96;
Sprite s_evo_sprites[SPECIES_EVO_MAX];
bool s_evo_loaded;

lv_obj_t *s_name;  // nome no cabeçalho (só Status/Evolução)
lv_obj_t *s_panels[kTabCount];
lv_obj_t *s_tabs[kTabCount];
lv_obj_t *s_evo_imgs[SPECIES_EVO_MAX];
uint16_t s_evo_numbers[SPECIES_EVO_MAX];
uint8_t s_evo_count;

const StrId kTabLabels[kTabCount] = {STR_DETAIL_TAB_INFO, STR_DETAIL_TAB_STATS,
                                     STR_DETAIL_TAB_EVOLUTION};
const StrId kStatLabels[SPECIES_STAT_COUNT] = {STR_STAT_HP,  STR_STAT_ATK, STR_STAT_DEF,
                                               STR_STAT_SPA, STR_STAT_SPD, STR_STAT_SPE};

void free_sprites() {
  sprite_free(&s_sprite96);
  for (Sprite &s : s_evo_sprites) sprite_free(&s);
}

void open_species(uint16_t n, DetailTab tab) {
  CatalogState &cs = catalog_state();
  cs.selected = n;
  cs.tab = tab;
  nav_go(Screen::Detail);
}

// ---------- cabeçalho ----------

void on_back(lv_event_t *) {
  CatalogState &cs = catalog_state();
  if (cs.back != Screen::Boot) {
    Screen to = cs.back;
    cs.back = Screen::Boot;
    nav_go(to);
    return;
  }
  cs.anchor = s_n;  // volta na página da espécie que estava aberta
  nav_go(cs.view);
}

void on_neighbor(lv_event_t *e) {
  uint16_t n = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
  DetailTab tab = catalog_state().tab;
  if (!status_store_get().is_seen(n)) tab = DetailTab::Info;
  open_species(n, tab);
}

void header_nav_button(lv_obj_t *h, const lv_img_dsc_t *icon, uint16_t target) {
  lv_obj_t *b = ui_header_button(h, icon, on_neighbor, (void *)(uintptr_t)target);
  if (target == 0) {
    lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_recolor(lv_obj_get_child(b, 0), c(theme::FAINT_2), 0);
  }
}

// ---------- abas ----------

void ensure_evo_sprites();

void show_tab(DetailTab tab) {
  catalog_state().tab = tab;
  for (uint8_t i = 0; i < kTabCount; i++) {
    bool on = i == (uint8_t)tab;
    if (on) lv_obj_clear_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_panels[i], LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *t = s_tabs[i];
    lv_obj_t *label = lv_obj_get_child(t, 0);
    lv_obj_t *bar = lv_obj_get_child(t, 1);
    bool disabled = s_unknown && i != 0;
    lv_obj_set_style_text_font(label, on ? &font_14_700 : &font_14_600, 0);
    lv_obj_set_style_text_color(
        label, c(on ? theme::ACCENT : (disabled ? theme::FAINT : theme::TEXT)), 0);
    if (on) lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
  }
  if (tab == DetailTab::Info) lv_obj_add_flag(s_name, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(s_name, LV_OBJ_FLAG_HIDDEN);
  if (tab == DetailTab::Evolution) ensure_evo_sprites();
}

void on_tab(lv_event_t *e) {
  DetailTab tab = (DetailTab)(uintptr_t)lv_event_get_user_data(e);
  if (tab == catalog_state().tab) return;
  show_tab(tab);
}

void build_tabs(lv_obj_t *scr) {
  lv_obj_t *bar = ui_box(scr);
  lv_obj_set_pos(bar, 0, kTabsY);
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
    // Faixa accent de 3 px no topo da aba ativa (inset do canvas).
    lv_obj_t *mark = ui_box(t);
    lv_obj_set_size(mark, LV_PCT(100), 3);
    lv_obj_set_style_bg_color(mark, c(theme::ACCENT), 0);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
    if (!(s_unknown && i != 0)) ui_on_click(t, on_tab, (void *)(uintptr_t)i);
    s_tabs[i] = t;
  }
}

lv_obj_t *panel(lv_obj_t *scr) {
  lv_obj_t *p = ui_box(scr);
  lv_obj_set_pos(p, 0, kBodyY);
  lv_obj_set_size(p, 320, kBodyH);
  return p;
}

// ---------- Info ----------

// Linhas de 26 px; com a 4ª ("Onde está") ficam com 22 pra caber (canvas).
lv_coord_t s_row_h = 26;

lv_obj_t *info_row(lv_obj_t *dl, uint8_t i, StrId label, const char *value, bool muted_value,
                   bool divider) {
  lv_obj_t *row = ui_box(dl);
  lv_obj_set_pos(row, 0, i * s_row_h);
  lv_obj_set_size(row, LV_PCT(100), s_row_h);
  if (divider) {
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, c(theme::ROW_DIVIDER), 0);
  }
  lv_obj_align(ui_label(row, &font_12_500, theme::MUTED, tr(label)), LV_ALIGN_LEFT_MID, 0, 0);
  if (value) {
    lv_obj_align(ui_label(row, &font_14_600, muted_value ? theme::MUTED : theme::TEXT, value),
                 LV_ALIGN_RIGHT_MID, 0, 0);
  }
  return row;
}

// ---------- Onde está (saves) ----------

SaveWhere s_where[16];
uint8_t s_where_n;

void on_where(lv_event_t *) {
  if (!s_where_n) return;
  SavesState &st = saves_state();
  const SaveWhere &w = s_where[0];
  st.save = w.save;
  if (w.place == SAVE_WHERE_PARTY) {
    st.tab = SaveTab::Party;
  } else {
    st.tab = SaveTab::Boxes;
    st.box = (uint8_t)(w.place - 1);
    st.box_slot = 0xFF;
    st.focus_species = s_n;
  }
  nav_go(Screen::Save);
}

void where_row(lv_obj_t *dl, uint8_t i) {
  lv_obj_t *row = info_row(dl, i, STR_DETAIL_WHERE, nullptr, false, false);
  lv_obj_t *val = ui_box(row);
  lv_obj_set_size(val, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(val, 4, 0);
  lv_obj_set_flex_flow(val, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(val, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  SaveHead hd;
  const SaveWhere &w = s_where[0];
  const char *who = save_store_head(w.save, &hd) ? hd.name : "?";
  char buf[40];
  if (w.place == SAVE_WHERE_PARTY) snprintf(buf, sizeof(buf), tr(STR_WHERE_PARTY_FMT), who);
  else snprintf(buf, sizeof(buf), tr(STR_WHERE_BOX_FMT), w.place, who);
  lv_obj_t *l = ui_label(val, &font_12_700, theme::TEXT, buf);
  lv_obj_set_style_max_width(l, 110, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  if (s_where_n > 1) {
    char more[8];
    snprintf(more, sizeof(more), "+%u", s_where_n - 1);
    lv_obj_t *chip = ui_chip(val, more, theme::SURFACE_2, theme::MUTED, 5);
    lv_obj_set_height(chip, 16);
  }
  lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(row, c(theme::SURFACE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
  ui_on_click(row, on_where);
}

void build_info(lv_obj_t *p) {
  SpeciesState state = status_store_get().state(s_n);

  // Sprite 112x112 com o 96 centralizado.
  lv_obj_t *frame = ui_box(p);
  lv_obj_set_pos(frame, 8, 8);
  lv_obj_set_size(frame, 112, 112);
  lv_obj_set_style_radius(frame, 10, 0);
  lv_obj_set_style_bg_color(frame, c(s_unknown ? theme::SURFACE_DIM : theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
  lv_obj_t *img = lv_img_create(frame);
  if (!s_unknown && s_have_rec && sprite_load(s_n, SpriteSize::S96, &s_sprite96)) {
    lv_img_set_src(img, &s_sprite96.dsc);
  } else {
    lv_img_set_src(img, &icon_dash_box_96);
    lv_obj_set_style_img_recolor(img, c(theme::FAINT_2), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    if (s_unknown) lv_obj_center(ui_label(frame, &font_28_700, theme::FAINT, "?"));
  }
  lv_obj_center(img);

  // Selo de status 112x24.
  lv_obj_t *pill = ui_box(p);
  lv_obj_set_pos(pill, 8, 8 + 112 + 6);
  lv_obj_set_size(pill, 112, 24);
  lv_obj_set_style_radius(pill, 12, 0);
  StrId pill_text;
  uint32_t pill_color;
  if (state == SpeciesState::Caught) {
    lv_obj_set_style_bg_color(pill, c(theme::ACCENT_SOFT), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    pill_text = STR_STATE_CAUGHT;
    pill_color = theme::ACCENT;
  } else if (state == SpeciesState::Seen) {
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, c(theme::LINE), 0);
    pill_text = STR_STATE_SEEN;
    pill_color = theme::TEXT;
  } else {
    // O LVGL 8 não faz borda tracejada: a pílula vem pronta como ícone A8.
    lv_obj_t *dash = ui_icon(pill, &icon_dash_pill_112, theme::FAINT_2);
    lv_obj_center(dash);
    pill_text = STR_STATE_UNSEEN;
    pill_color = theme::MUTED;
  }
  lv_obj_t *row = ui_box(pill);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_status_mark_set(ui_status_mark(row), state);
  ui_label(row, &font_12_700, pill_color, tr(pill_text));
  lv_obj_center(row);

  // Coluna da direita: x 130, largura 182.
  constexpr lv_coord_t kX = 8 + 112 + 10, kW = 320 - kX - 8;
  lv_obj_t *name = ui_label(p, &font_20_700, s_unknown ? theme::MUTED : theme::TEXT,
                            s_unknown ? tr(STR_UNKNOWN_NAME) : species_repo_name(s_n));
  lv_obj_set_pos(name, kX, 8);
  lv_obj_set_width(name, kW);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

  lv_obj_t *chips = ui_box(p);
  lv_obj_set_pos(chips, kX, 8 + 24 + 6);
  lv_obj_set_size(chips, kW, 20);
  lv_obj_set_style_pad_column(chips, 4, 0);
  lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
  if (s_unknown) {
    ui_chip(chips, tr(STR_UNKNOWN_NAME), theme::SURFACE_2, theme::MUTED, 10);
  } else {
    SpeciesType t[2];
    species_repo_types(s_n, &t[0], &t[1]);
    for (SpeciesType ty : t) {
      if (ty != SpeciesType::None)
        ui_chip(chips, species_type_name(ty), species_type_color(ty), theme::ON_ACCENT);
    }
  }

  // "Onde está": só espécies que os saves podem ter (Gen3: até 386).
  s_where_n = s_unknown ? 0 : save_store_where(s_n, s_where, sizeof(s_where) / sizeof(s_where[0]));
  s_row_h = s_where_n ? 22 : 26;
  lv_obj_t *dl = ui_box(p);
  lv_obj_set_pos(dl, kX, 8 + 24 + 6 + 20 + 10);
  lv_obj_set_size(dl, kW, (s_where_n ? 4 : 3) * s_row_h);
  char num[12], height[20], weight[20];
  const char *dash = "—";
  bool measures = !s_unknown && s_have_rec;
  if (measures) {
    format_tenths(s_rec.height_dm, tr(STR_DECIMAL_SEP), num, sizeof(num));
    snprintf(height, sizeof(height), tr(STR_DETAIL_HEIGHT_FMT), num);
    format_tenths(s_rec.weight_hg, tr(STR_DECIMAL_SEP), num, sizeof(num));
    snprintf(weight, sizeof(weight), tr(STR_DETAIL_WEIGHT_FMT), num);
  }
  info_row(dl, 0, STR_DETAIL_HEIGHT, measures ? height : dash, !measures, true);
  info_row(dl, 1, STR_DETAIL_WEIGHT, measures ? weight : dash, !measures, true);
  info_row(dl, 2, STR_DETAIL_GENERATION, dex_generation_roman(dex_generation_of(s_n)), false,
           s_where_n > 0);
  if (s_where_n) where_row(dl, 3);
}

// ---------- Status ----------

void build_stats(lv_obj_t *p) {
  // padding 8/12; 6 linhas de 20 + total de 22 distribuídas em 154 px.
  constexpr lv_coord_t kPadX = 12, kPadY = 8, kRowH = 20, kStep = 22;
  constexpr lv_coord_t kLabelW = 34, kValueW = 28, kColGap = 8;
  constexpr lv_coord_t kValueX = kPadX + kLabelW + kColGap;
  constexpr lv_coord_t kBarX = kValueX + kValueW + kColGap;
  constexpr lv_coord_t kBarW = 320 - kPadX - kBarX;

  for (uint8_t i = 0; i < SPECIES_STAT_COUNT; i++) {
    lv_coord_t y = kPadY + i * kStep;
    uint8_t v = s_have_rec ? s_rec.stats[i] : 0;
    lv_obj_t *l = ui_label(p, &font_12_600, theme::MUTED, tr(kStatLabels[i]));
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, kPadX, y + (kRowH - 15) / 2);
    lv_obj_t *val = ui_label(p, &font_14_700, theme::TEXT, "");
    lv_label_set_text_fmt(val, "%u", v);
    lv_obj_set_width(val, kValueW);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(val, LV_ALIGN_TOP_LEFT, kValueX, y + (kRowH - 17) / 2);
    lv_obj_t *bar = ui_progress(p, kBarW, 8);
    lv_obj_set_pos(bar, kBarX, y + (kRowH - 8) / 2);
    // Escala até 150; >= 60 em accent (canvas).
    lv_obj_set_style_bg_color(bar, c(v >= 60 ? theme::ACCENT : theme::TEXT), LV_PART_INDICATOR);
    ui_progress_set(bar, v > 150 ? 150 : v, 150);
  }

  lv_coord_t ty = kPadY + SPECIES_STAT_COUNT * kStep;
  lv_obj_t *line = ui_box(p);
  lv_obj_set_pos(line, kPadX, ty);
  lv_obj_set_size(line, 320 - 2 * kPadX, 1);
  lv_obj_set_style_bg_color(line, c(theme::LINE), 0);
  lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
  lv_obj_t *tl = ui_label(p, &font_12_700, theme::TEXT, tr(STR_STAT_TOTAL));
  lv_obj_set_pos(tl, kPadX, ty + 5);
  lv_obj_t *tv = ui_label(p, &font_14_700, theme::TEXT, "");
  lv_label_set_text_fmt(tv, "%u", s_have_rec ? s_rec.stat_total() : 0);
  lv_obj_set_width(tv, kValueW + 20);  // total pode ter 3 dígitos largos (ex.: 720)
  lv_obj_set_style_text_align(tv, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(tv, kValueX - 20, ty + 4);
}

// ---------- Evolução ----------

void on_stage(lv_event_t *e) {
  uint16_t n = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
  if (n != s_n) open_species(n, DetailTab::Info);
}

void ensure_evo_sprites() {
  if (s_evo_loaded) return;
  s_evo_loaded = true;
  uint32_t t0 = lv_tick_get();
  const DexStatus &st = status_store_get();
  for (uint8_t i = 0; i < s_evo_count; i++) {
    uint16_t n = s_evo_numbers[i];
    if (st.is_seen(n) && sprite_load(n, SpriteSize::S64, &s_evo_sprites[i])) {
      lv_obj_set_style_img_recolor_opa(s_evo_imgs[i], LV_OPA_TRANSP, 0);
      lv_img_set_src(s_evo_imgs[i], &s_evo_sprites[i].dsc);
    }
  }
  LOG_D("detail", "evolução: %u sprites em %ums, maior_bloco=%u", s_evo_count,
        lv_tick_elaps(t0), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void build_stage(lv_obj_t *p, lv_coord_t x, lv_coord_t y, uint8_t i) {
  uint16_t n = s_evo_numbers[i];
  SpeciesState state = status_store_get().state(n);
  bool unknown = state == SpeciesState::Unknown;
  bool current = n == s_n;

  lv_obj_t *col = ui_box(p);
  lv_obj_set_pos(col, x, y);
  lv_obj_set_size(col, kStageW, 76 + 4 + 16 + 2 + 15);
  ui_on_click(col, on_stage, (void *)(uintptr_t)n);

  lv_obj_t *card = ui_box(col);
  lv_obj_set_size(card, kStageW, 76);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_style_bg_color(card, c(unknown ? theme::SURFACE_DIM : theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, current ? 2 : 1, 0);
  lv_obj_set_style_border_color(card, c(current ? theme::ACCENT : theme::LINE), 0);
  lv_obj_set_style_bg_color(card, c(theme::SURFACE_2), LV_STATE_PRESSED);

  // Caixa tracejada até o sprite chegar (ensure_evo_sprites).
  lv_obj_t *img = lv_img_create(card);
  lv_img_set_src(img, &icon_dash_box_64);
  lv_obj_set_style_img_recolor(img, c(theme::FAINT_2), 0);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_center(img);
  s_evo_imgs[i] = img;
  if (unknown) lv_obj_center(ui_label(card, &font_20_700, theme::FAINT, "?"));

  lv_obj_t *row = ui_box(col);
  lv_obj_set_size(row, LV_SIZE_CONTENT, 16);
  lv_obj_set_style_max_width(row, kStageW, 0);
  lv_obj_set_style_pad_column(row, 4, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_status_mark_set(ui_status_mark(row, 12), state);
  lv_obj_t *name = ui_label(row, &font_12_700, unknown ? theme::MUTED : theme::TEXT,
                            unknown ? tr(STR_UNKNOWN_NAME) : species_repo_name(n));
  lv_obj_set_style_max_width(name, kStageW - 16, 0);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 76 + 4);

  lv_obj_t *num = ui_label(col, &font_12_500, theme::MUTED, "");
  lv_label_set_text_fmt(num, "#%03u", n);
  lv_obj_align(num, LV_ALIGN_TOP_MID, 0, 76 + 4 + 16 + 2);
}

void build_evolution(lv_obj_t *p) {
  s_evo_count = 0;
  if (s_have_rec) {
    for (uint8_t i = 0; i < s_rec.evo_count && i < SPECIES_EVO_MAX; i++) {
      if (s_rec.evo[i]) s_evo_numbers[s_evo_count++] = s_rec.evo[i];
    }
  }
  if (s_evo_count == 0) s_evo_numbers[s_evo_count++] = s_n;

  constexpr lv_coord_t kBlockH = 76 + 4 + 16 + 2 + 15;
  lv_coord_t total_w = s_evo_count * kStageW + (s_evo_count - 1) * (kArrowW + 2 * kEvoGap);
  lv_coord_t x = (320 - total_w) / 2;
  lv_coord_t y = (kBodyH - kBlockH) / 2;
  if (s_evo_count == 1) y -= 10;  // espaço pro "Não evolui"

  for (uint8_t i = 0; i < s_evo_count; i++) {
    build_stage(p, x, y, i);
    x += kStageW;
    if (i + 1 == s_evo_count) break;
    x += kEvoGap;
    // Seta + nível, alinhada ao meio do card.
    lv_obj_t *arrow = ui_box(p);
    lv_obj_set_size(arrow, kArrowW, 18 + 2 + 15);
    lv_obj_set_pos(arrow, x, y + 38 - 12);
    lv_obj_align(ui_icon(arrow, &icon_arrow_18, theme::MUTED), LV_ALIGN_TOP_MID, 0, 0);
    uint8_t level = s_rec.evo_level[i];
    if (level) {
      lv_obj_t *l = ui_label(arrow, &font_12_600, theme::MUTED, "");
      lv_label_set_text_fmt(l, tr(STR_EVO_LEVEL_FMT), level);
      lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 20);
    }
    x += kArrowW + kEvoGap;
  }

  if (s_evo_count == 1) {
    lv_obj_t *l = ui_label(p, &font_12_500, theme::MUTED, tr(STR_EVO_NONE));
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y + kBlockH + 8);
  }
}

void on_delete(lv_event_t *) { free_sprites(); }

}  // namespace

lv_obj_t *detail_screen_create() {
  CatalogState &cs = catalog_state();
  s_n = cs.selected;
  s_unknown = !status_store_get().is_seen(s_n);
  s_have_rec = !s_unknown && species_repo_get(s_n, &s_rec);
  s_evo_loaded = false;
  if (s_unknown) cs.tab = DetailTab::Info;

  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);

  char title[8];
  snprintf(title, sizeof(title), "#%03u", s_n);
  lv_obj_t *h = ui_header(scr, title, on_back);
  s_name = ui_label(h, &font_14_600, theme::TEXT, s_unknown ? "" : species_repo_name(s_n));
  lv_obj_set_style_pad_left(s_name, 6, 0);
  lv_obj_set_style_max_width(s_name, 150, 0);
  lv_label_set_long_mode(s_name, LV_LABEL_LONG_DOT);
  ui_header_spacer(h);
  uint16_t len, prev, next;
  const uint16_t *list = catalog_list(&len);
  catalog_neighbors(list, len, s_n, &prev, &next);
  header_nav_button(h, &icon_prev_18, prev);
  header_nav_button(h, &icon_forward_18, next);

  // O 96 primeiro: é o maior bloco.
  s_panels[0] = panel(scr);
  build_info(s_panels[0]);
  s_panels[1] = panel(scr);
  build_stats(s_panels[1]);
  s_panels[2] = panel(scr);
  build_evolution(s_panels[2]);
  build_tabs(scr);
  show_tab(cs.tab);
  return scr;
}
