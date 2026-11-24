// 14 Modo vitrine (canvas: Vitrine.dc.html, "carta"; CLAUDE.md "Modo vitrine").
//   cartão 314x234 sem moldura (3 px da borda da tela, padding 3/5):
//     cabeçalho 22: nome (16 px, 1 linha, "…") e tipos abreviados à direita
//     corpo: janela 148x148 (borda 2 na cor do tipo 1 a 50% sobre o fundo; com 2 tipos,
//       esquerda/baixo no tipo 1 e topo/direita no tipo 2) com o sprite 96 em 2x
//       cortado em 144 (72 px do original, centrados na parte opaca; mais alto
//       que 72, ancora embaixo) · medidas + 6 status com rótulo, valor e barra
//     rodapé 18: "#nº · Geração X" e o status de captura
//   Mostrar "Todos" inclui as não vistas: só nº, nome e sprite; tipos, medidas
//     e valores viram "???", borda neutra (canvas: tweak naoVisto)
//   Mostrar "Meu time": o time de todos os saves (resumos do saves.cache). O
//     nome vira o apelido + "Nv", o rodapé "#nº · Espécie", o save e a ★;
//     tipos, medidas e status continuam os da espécie
//   tocar em qualquer lugar volta pro Menu
// 14b: indicadores do PUT /status (notify/Status.h) em chips no rodapé, antes
//   da marca de status; com chip, a marca fica só o círculo. Sobrevivem à
//   troca de espécie e só são redesenhados quando a versão do quadro muda.
// 15c: balão de aviso com `actions` ganha "›" e abre a folha de resposta
//   (Negar / Permitir, que só aceita toque 1 s depois de abrir). Tocar fora
//   fecha sem responder. Com a folha aberta a espécie não troca.
// Aleatória sem repetir até mostrar todas (showcase_pick, "saco" por número).
// Só redesenha na troca (timer do intervalo dos Ajustes); entre trocas nada
// muda. Cada troca desloca tudo 1–2 px (anti-retenção). Brilho próprio
// enquanto a tela existe; a tela nunca apaga aqui (ui/power).
#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "data/SaveStore.h"
#include "data/Settings.h"
#include "data/SpeciesRepo.h"
#include "data/SpriteLoader.h"
#include "data/StatusStore.h"
#include "dex/Dex.h"
#include "i18n/i18n.h"
#include "logic/Format.h"
#include "logic/Showcase.h"
#include "net/notice_server.h"
#include "notify/Notice.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/notice_center.h"
#include "ui/power.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "util/log.h"

namespace {

// Cartão (posição sem o deslocamento) e o miolo dele, sem o padding 6/10/6.
constexpr lv_coord_t kCardX = 3, kCardY = 3, kCardW = 314, kCardH = 234;
constexpr lv_coord_t kPadV = 3, kPadH = 5, kGap = 4;
constexpr lv_coord_t kInnerW = kCardW - 2 * kPadH;  // 304
// Rodapé: 18 de conteúdo + 4 de padding + 1 de borda (content-box no canvas).
constexpr lv_coord_t kHeadH = 22, kFootH = 23;
constexpr lv_coord_t kBodyY = kHeadH + kGap;
constexpr lv_coord_t kBodyH = kCardH - 2 * kPadV - kHeadH - kFootH - 2 * kGap;  // 175
constexpr lv_coord_t kWinW = 148, kWinBorder = 2;  // janela de 144 por dentro
constexpr lv_coord_t kWinY = kBodyY + (kBodyH - kWinW) / 2;
constexpr lv_coord_t kColX = kWinW + 8, kColW = kInnerW - kColX;  // 148
constexpr lv_coord_t kStatH = 14, kStatGap = 3, kStatLabelW = 26, kStatValueW = 24;
constexpr lv_coord_t kStatBarX = kStatLabelW + 4 + kStatValueW + 4, kStatBarH = 4;
constexpr lv_coord_t kChipPad = 7, kChipGap = 4;
constexpr uint16_t kChromaKey = 0x07E0;  // lv_color_hex(0x00FF00) em RGB565 (tools/build_sd.py)

uint16_t s_list[DEX_COUNT];  // espécies elegíveis (2 KB); no "Meu time", posições 1..n
uint16_t s_count;
// "Meu time": posição (1..n) -> save << 8 | vaga. O saco sorteia por posição,
// pra duas criaturas da mesma espécie não contarem como uma.
constexpr uint16_t kTeamMax = 16 * SAVE_PARTY_MAX;
uint16_t s_team[kTeamMax];
bool s_team_mode;
uint8_t s_last_src = 0xFF;  // trocou a fonte: o saco recomeça
int32_t s_pos = -1;      // sobrevive entre sessões: a sequencial continua de onde parou
// Aleatória: espécies que já saíram na rodada (saco, só RAM) e a última mostrada.
// Também sobrevivem entre sessões; reiniciar o aparelho começa outra rodada.
uint8_t s_shown[DEX_BITS_BYTES];
uint16_t s_last_n;
uint32_t s_shift_k;
Sprite s_sprite;
lv_obj_t *s_card;
lv_obj_t *s_win;  // janela do sprite da troca atual (filha do s_card)
lv_timer_t *s_timer;
lv_obj_t *s_scr;
// Indicadores (14b): contêiner no rodapé da troca atual e a palavra de status.
lv_obj_t *s_chips;
lv_obj_t *s_state_word;
uint32_t s_chips_version;
lv_timer_t *s_watch;  // 250 ms: indicadores mudaram? a folha ainda vale?
// Folha de resposta (15c).
lv_obj_t *s_wrap;      // balões atuais (escondidos com a folha aberta)
lv_obj_t *s_sheet;     // nullptr = fechada
lv_obj_t *s_catch;     // tela inteira por baixo da folha: toque fora fecha
lv_obj_t *s_allow;
lv_timer_t *s_arm;
bool s_armed;
char s_ask_id[sizeof(Notice::id)];
uint32_t s_ask_rev;
uint32_t s_ask_opened;
constexpr uint32_t kArmMs = 1000;
constexpr uint32_t kSheetMaxMs = 60000;  // sem toque: fecha sozinha

const StrId kStatLabels[SPECIES_STAT_COUNT] = {STR_STAT_HP,  STR_STAT_ATK, STR_STAT_DEF,
                                               STR_STAT_SPA, STR_STAT_SPD, STR_STAT_SPE};

void build_list() {
  const DexStatus &st = status_store_get();
  ShowcaseSource src = (ShowcaseSource)settings().showcase_source;
  s_count = 0;
  if ((uint8_t)src != s_last_src) {
    memset(s_shown, 0, sizeof(s_shown));
    s_last_n = 0;
    s_pos = -1;
    s_last_src = (uint8_t)src;
  }
  s_team_mode = src == ShowcaseSource::Team;
  if (s_team_mode) {
    SaveHead h;
    for (uint8_t i = 0; i < save_store_count(); i++) {
      if (!save_store_head(i, &h)) continue;
      for (uint8_t k = 0; k < h.party_count && s_count < kTeamMax; k++) {
        s_team[s_count] = (uint16_t)(i << 8 | k);
        s_list[s_count] = s_count + 1;
        s_count++;
      }
    }
    return;
  }
  for (uint16_t n = 1; n <= species_repo_count(); n++) {
    if (showcase_eligible(src, st.is_seen(n), st.is_caught(n))) s_list[s_count++] = n;
  }
}

lv_coord_t chip_w(const char *text) {
  return lv_txt_get_width(text, strlen(text), &font_12_700, 0, LV_TEXT_FLAG_NONE) + 2 * kChipPad;
}

// Criatura do "Meu time" sendo mostrada (vazio fora dele).
SavePartyBrief s_brief;
SaveHead s_brief_save;

// Cabeçalho: tipos à direita, o nome fica com o que sobra.
void build_header(uint16_t n, const SpeciesRecord *rec, bool unseen) {
  lv_obj_t *chips = ui_box(s_card);
  lv_obj_set_size(chips, LV_SIZE_CONTENT, 20);
  lv_obj_set_style_pad_column(chips, kChipGap, 0);
  lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
  lv_obj_align(chips, LV_ALIGN_TOP_RIGHT, 0, (kHeadH - 20) / 2);
  lv_coord_t used = 0;
  if (unseen) {
    const char *q = tr(STR_UNKNOWN_NAME);
    lv_obj_t *chip = ui_chip(chips, q, theme::BG, theme::MUTED, kChipPad);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, c(theme::FAINT), 0);
    used = chip_w(q) + 2;  // + borda
  } else if (rec) {
    for (SpeciesType t : {rec->type1, rec->type2}) {
      if (t == SpeciesType::None) continue;
      const char *ab = species_type_abbr(t);
      ui_chip(chips, ab, species_type_color(t), theme::ON_ACCENT, kChipPad);
      used += (used ? kChipGap : 0) + chip_w(ab);
    }
  }

  lv_coord_t free_w = kInnerW - used - (used ? 8 : 0);
  if (s_team_mode) {
    // Apelido + "Nv 58" logo depois (o nível nunca é cortado).
    lv_obj_t *row = ui_box(s_card);
    lv_obj_set_size(row, free_w, kHeadH);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    char lv[12];
    snprintf(lv, sizeof(lv), tr(STR_EVO_LEVEL_FMT), s_brief.level);
    lv_coord_t lv_w = lv_txt_get_width(lv, strlen(lv), &font_12_600, 0, LV_TEXT_FLAG_NONE);
    lv_obj_t *nl = ui_label(row, &font_16_700, theme::TEXT, s_brief.nick);
    ui_label_clamp(nl, free_w - lv_w - 8, 1);
    ui_label(row, &font_12_600, theme::MUTED, lv);
    return;
  }
  lv_obj_t *nl = ui_label(s_card, &font_16_700, theme::TEXT, species_repo_name(n));
  ui_label_clamp(nl, free_w, 1);
  lv_obj_align(nl, LV_ALIGN_TOP_LEFT, 0,
               (kHeadH - lv_font_get_line_height(&font_16_700)) / 2);
}

// Janela 148x148: borda na cor do tipo 1 (desenhada por cima do sprite) e o
// sprite 96 em 2x preso no canto, com a origem do corte em (ox, oy).
void build_window(uint16_t n, const SpeciesRecord *rec, bool unseen) {
  // Cor do tipo meio a meio com o fundo: a moldura não compete com o sprite.
  lv_color_t tint = unseen || !rec ? c(theme::FAINT)
                                   : lv_color_mix(c(species_type_color(rec->type1)), c(theme::BG), LV_OPA_50);
  lv_obj_t *win = ui_box(s_card);
  s_win = win;
  lv_obj_set_size(win, kWinW, kWinW);
  lv_obj_set_pos(win, 0, kWinY);
  lv_obj_set_style_radius(win, 8, 0);
  lv_obj_set_style_clip_corner(win, true, 0);
  lv_obj_set_style_bg_color(win, c(theme::BG), 0);
  lv_obj_set_style_bg_opa(win, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(win, kWinBorder, 0);
  lv_obj_set_style_border_color(win, tint, 0);
  lv_obj_set_style_border_post(win, true, 0);

  if (!sprite_load(n, SpriteSize::S96, &s_sprite)) return;
  const uint16_t side = s_sprite.dsc.header.w;
  uint8_t x0 = 0, x1 = side - 1, y0 = 0, y1 = side - 1;
  showcase_opaque_box((const uint16_t *)s_sprite.dsc.data, side, kChromaKey, &x0, &x1, &y0, &y1);
  uint8_t ox = showcase_crop_origin(x0, x1, side, SHOWCASE_CROP, false);
  uint8_t oy = showcase_crop_origin(y0, y1, side, SHOWCASE_CROP, true);

  lv_obj_t *img = lv_img_create(win);
  lv_img_set_src(img, &s_sprite.dsc);
  lv_img_set_antialias(img, false);
  lv_img_set_pivot(img, 0, 0);
  lv_img_set_zoom(img, 512);
  // Posição relativa ao miolo da janela (dentro da borda).
  lv_obj_set_pos(img, -2 * ox, -2 * oy);
}

// Dois tipos: a janela fica com a borda inteira no tipo 1 e um anel por cima,
// no mesmo lugar, com a borda inteira no tipo 2 recortada por uma máscara na
// diagonal (canto inferior esquerdo → superior direito): só a metade de cima
// fica, e as cores se encontram no meio das curvas superior-esquerda e
// inferior-direita. (Borda parcial com raio no LVGL 8.3 não desenha a curva.)
// Não clicável (ui_box): o toque vai pra tela.
lv_draw_mask_line_param_t s_ring_mask;  // precisa viver enquanto o anel desenha
int16_t s_ring_mask_id = LV_MASK_ID_INV;

void on_ring_draw(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_DRAW_MAIN_BEGIN) {
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target(e), &a);
    lv_draw_mask_line_points_init(&s_ring_mask, a.x1, a.y2, a.x2, a.y1,
                                  LV_DRAW_MASK_LINE_SIDE_TOP);
    s_ring_mask_id = lv_draw_mask_add(&s_ring_mask, nullptr);
  } else if (code == LV_EVENT_DRAW_MAIN_END && s_ring_mask_id != LV_MASK_ID_INV) {
    lv_draw_mask_free_param(&s_ring_mask);
    lv_draw_mask_remove_id(s_ring_mask_id);
    s_ring_mask_id = LV_MASK_ID_INV;
  }
}

void build_window_ring(const SpeciesRecord *rec, bool unseen) {
  if (unseen || !rec || rec->type2 == SpeciesType::None) return;
  lv_obj_t *ring = ui_box(s_card);
  lv_obj_set_size(ring, kWinW, kWinW);
  lv_obj_set_pos(ring, 0, kWinY);
  lv_obj_set_style_radius(ring, 8, 0);
  lv_obj_set_style_border_width(ring, kWinBorder, 0);
  lv_obj_set_style_border_color(
      ring, lv_color_mix(c(species_type_color(rec->type2)), c(theme::BG), LV_OPA_50), 0);
  lv_obj_add_event_cb(ring, on_ring_draw, LV_EVENT_DRAW_MAIN_BEGIN, nullptr);
  lv_obj_add_event_cb(ring, on_ring_draw, LV_EVENT_DRAW_MAIN_END, nullptr);
}

void build_stats(const SpeciesRecord *rec, bool unseen) {
  lv_obj_t *col = ui_box(s_card);
  lv_obj_set_pos(col, kColX, kBodyY);
  lv_obj_set_size(col, kColW, kBodyH);
  lv_obj_set_style_pad_row(col, 6, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  char line[44];
  if (unseen || !rec) {
    snprintf(line, sizeof(line), "%s m · %s kg", tr(STR_UNKNOWN_NAME), tr(STR_UNKNOWN_NAME));
  } else {
    char h[12], w[12], hs[20], ws[20];
    format_tenths(rec->height_dm, tr(STR_DECIMAL_SEP), h, sizeof(h));
    format_tenths(rec->weight_hg, tr(STR_DECIMAL_SEP), w, sizeof(w));
    snprintf(hs, sizeof(hs), tr(STR_DETAIL_HEIGHT_FMT), h);
    snprintf(ws, sizeof(ws), tr(STR_DETAIL_WEIGHT_FMT), w);
    snprintf(line, sizeof(line), "%s · %s", hs, ws);
  }
  ui_label(col, &font_12_500, theme::MUTED, line);

  // PS..Vel: rótulo, valor e barra na escala 150, >= 60 em accent.
  lv_obj_t *rows = ui_box(col);
  lv_obj_set_size(rows, kColW, SPECIES_STAT_COUNT * kStatH + (SPECIES_STAT_COUNT - 1) * kStatGap);
  for (int i = 0; i < SPECIES_STAT_COUNT; i++) {
    lv_coord_t y = i * (kStatH + kStatGap);
    uint8_t v = rec && !unseen ? rec->stats[i] : 0;
    lv_obj_t *l = ui_label(rows, &font_12_600, theme::MUTED, tr(kStatLabels[i]));
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_t *val = ui_label(rows, &font_12_700, theme::TEXT, "");
    if (unseen || !rec) lv_label_set_text(val, tr(STR_UNKNOWN_NAME));
    else lv_label_set_text_fmt(val, "%u", v);
    lv_obj_set_width(val, kStatValueW);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(val, LV_ALIGN_TOP_LEFT, kStatLabelW + 4, y);
    lv_obj_t *bar = ui_progress(rows, kColW - kStatBarX, kStatBarH);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, kStatBarX, y + (kStatH - kStatBarH) / 2);
    lv_obj_set_style_bg_color(bar, c(v >= 60 ? theme::ACCENT : theme::TEXT), LV_PART_INDICATOR);
    ui_progress_set(bar, v > 150 ? 150 : v, 150);  // não vista: trilho vazio
  }
}

// Chips dos indicadores (14b): ícone opcional, texto curto e barra opcional
// de 28x4 (o que sobra, na cor do indicador). Com algum, a palavra de status
// some e fica só o círculo.
void refresh_chips() {
  if (!s_chips) return;
  const StatusBoard &b = statuses();
  s_chips_version = b.version();
  lv_obj_clean(s_chips);
  for (uint8_t i = 0; i < b.size(); i++) {
    const StatusItem &it = b.at(i);
    lv_obj_t *chip = ui_box(s_chips);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 16);
    lv_obj_set_style_radius(chip, 8, 0);
    lv_obj_set_style_pad_hor(chip, 6, 0);
    lv_obj_set_style_pad_column(chip, 4, 0);
    lv_obj_set_style_bg_color(chip, c(theme::SURFACE_2), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (it.has_icon) ui_icon(chip, notice_icon_12(it.icon), it.color);
    // Com barra o texto é só o rótulo dela ("5h"): mais leve.
    ui_label(chip, it.bar >= 0 ? &font_12_600 : &font_12_700, it.text_color, it.text);
    if (it.bar >= 0) {
      lv_obj_t *bar = ui_progress(chip, 28, 4);
      lv_obj_set_style_bg_color(bar, c(theme::BG), 0);
      lv_obj_set_style_bg_color(bar, c(it.color), LV_PART_INDICATOR);
      ui_progress_set(bar, it.bar, 100);
    }
  }
  if (b.size()) lv_obj_clear_flag(s_chips, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_chips, LV_OBJ_FLAG_HIDDEN);
  if (s_state_word) {
    if (b.size()) lv_obj_add_flag(s_state_word, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(s_state_word, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t *build_chips(lv_obj_t *foot) {
  s_chips = ui_box(foot);
  lv_obj_set_size(s_chips, LV_SIZE_CONTENT, 16);
  lv_obj_set_style_pad_column(s_chips, 4, 0);
  lv_obj_set_flex_flow(s_chips, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_chips, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return s_chips;
}

void build_footer(uint16_t n, SpeciesState state) {
  lv_obj_t *foot = ui_box(s_card);
  lv_obj_set_size(foot, kInnerW, kFootH);
  lv_obj_align(foot, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(foot, 1, 0);
  lv_obj_set_style_border_color(foot, c(theme::LINE), 0);
  lv_obj_set_style_pad_top(foot, 4, 0);
  lv_obj_set_style_pad_column(foot, 6, 0);
  lv_obj_set_flex_flow(foot, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(foot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // O texto da esquerda cede espaço aos chips: uma linha só, com "…".
  lv_obj_t *id = ui_label(foot, &font_12_600, theme::MUTED, "");
  lv_obj_set_height(id, lv_font_get_line_height(&font_12_600));
  lv_label_set_long_mode(id, LV_LABEL_LONG_DOT);
  s_state_word = nullptr;
  if (s_team_mode) {
    lv_label_set_text_fmt(id, tr(STR_SC_TEAM_FOOTER_FMT), n, species_repo_name(n));
    lv_obj_set_flex_grow(id, 1);
    char who[48];
    snprintf(who, sizeof(who), tr(STR_HOF_SAVE_FMT), s_brief_save.name, save_game_name(s_brief_save.game));
    ui_label(foot, &font_12_600, theme::MUTED, who);
    if (s_brief.flags & SAVE_MON_SHINY) ui_icon(foot, &icon_star_12, theme::NOTICE_IDLE);
    build_chips(foot);
    refresh_chips();
    return;
  }
  lv_label_set_text_fmt(id, tr(STR_SC_FOOTER_FMT), n, dex_generation_roman(dex_generation_of(n)));
  lv_obj_set_flex_grow(id, 1);

  bool caught = state == SpeciesState::Caught;
  bool unseen = state == SpeciesState::Unknown;
  build_chips(foot);
  ui_status_mark_set(ui_status_mark(foot, 12), state);
  s_state_word = ui_label(foot, caught ? &font_12_700 : &font_12_600,
                          caught ? theme::ACCENT : unseen ? theme::MUTED : theme::TEXT,
                          tr(caught ? STR_STATE_CAUGHT : unseen ? STR_STATE_UNSEEN : STR_STATE_SEEN));
  refresh_chips();
}

void show(uint16_t n) {
  uint32_t t0 = lv_tick_get();
  if (s_team_mode) {
    // n = posição no time: troca pelo nº da espécie daquela vaga.
    uint16_t ref = s_team[n - 1];
    memset(&s_brief, 0, sizeof(s_brief));
    if (!save_store_party_brief((uint8_t)(ref >> 8), (uint8_t)ref, &s_brief) ||
        !save_store_head((uint8_t)(ref >> 8), &s_brief_save) || s_brief.species == 0)
      return;
    n = s_brief.species;
  }
  // O cartão sai antes do sprite: o lv_img ainda aponta pro buffer.
  s_chips = nullptr;
  s_state_word = nullptr;
  lv_obj_clean(s_card);
  sprite_free(&s_sprite);

  SpeciesRecord rec;
  bool have = species_repo_get(n, &rec);
  SpeciesState state = status_store_get().state(n);
  if (s_team_mode) state = SpeciesState::Caught;  // está no time: os dados aparecem
  bool unseen = state == SpeciesState::Unknown;  // só com Mostrar "Todos"
  int8_t dx, dy;
  showcase_shift(s_shift_k++, &dx, &dy);
  lv_obj_set_pos(s_card, kCardX + dx, kCardY + dy);

  const SpeciesRecord *r = have ? &rec : nullptr;
  build_header(n, r, unseen);
  build_window(n, r, unseen);
  build_window_ring(r, unseen);  // depois da janela: desenha por cima do sprite
  build_stats(r, unseen);
  build_footer(n, state);

  LOG_D("showcase", "#%u em %ums, maior_bloco=%u", n, lv_tick_elaps(t0),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void next() {
  if (settings().showcase_random) {
    s_pos = showcase_pick(s_list, s_count, s_shown, s_last_n, esp_random());
  } else {
    s_pos = showcase_next(s_pos < s_count ? s_pos : -1, s_count, false, 0);
  }
  s_last_n = s_list[s_pos];
  show(s_last_n);
}

void on_tick(lv_timer_t *) { next(); }

void on_exit(lv_event_t *) { nav_go(Screen::Menu); }

// ---------- Folha de resposta (15c) ----------

void sheet_close() {
  if (s_arm) lv_timer_del(s_arm);
  s_arm = nullptr;
  // Async: quem chama costuma ser o clique de um botão da própria folha.
  if (s_sheet) lv_obj_del_async(s_sheet);
  if (s_catch) lv_obj_del_async(s_catch);
  s_sheet = s_catch = s_allow = nullptr;
  s_armed = false;
  if (s_wrap) lv_obj_clear_flag(s_wrap, LV_OBJ_FLAG_HIDDEN);
  if (s_timer) {
    lv_timer_reset(s_timer);  // a espécie fica o intervalo inteiro depois de fechar
    lv_timer_resume(s_timer);
  }
}

// Tocar fora (ou a folha passou do tempo): fecha sem responder.
void on_sheet_outside(lv_event_t *) { sheet_close(); }

void on_sheet_answer(lv_event_t *e) {
  bool allow = lv_event_get_user_data(e) != nullptr;
  if (allow && !s_armed) return;  // ainda tracejado: toque sem querer
  notices().answer(s_ask_id, s_ask_rev, allow ? NoticeAnswer::Allow : NoticeAnswer::Deny, millis());
  sheet_close();  // o quadro mudou: o notice_center redesenha os balões sem este
}

// Um redesenho só: o Permitir deixa de ser tracejado.
void on_arm(lv_timer_t *) {
  s_arm = nullptr;  // repeat_count 1: o LVGL apaga sozinho
  s_armed = true;
  if (!s_allow) return;
  lv_obj_clean(s_allow);
  lv_obj_set_style_bg_color(s_allow, c(theme::ACCENT), 0);
  lv_obj_set_style_bg_opa(s_allow, LV_OPA_COVER, 0);
  lv_obj_t *l = ui_label(s_allow, &font_14_700, theme::ON_ACCENT, tr(STR_SC_ASK_ALLOW));
  lv_obj_center(l);
}

lv_obj_t *sheet_button(lv_obj_t *row, lv_coord_t w) {
  lv_obj_t *b = ui_box(row);
  lv_obj_set_size(b, w, 40);
  lv_obj_set_style_radius(b, 8, 0);
  return b;
}

void sheet_open(const Notice &n, uint32_t rev) {
  if (s_sheet || !s_scr) return;
  memcpy(s_ask_id, n.id, sizeof(s_ask_id));
  s_ask_rev = rev;
  s_ask_opened = millis();
  s_armed = false;
  if (s_wrap) lv_obj_add_flag(s_wrap, LV_OBJ_FLAG_HIDDEN);
  if (s_timer) lv_timer_pause(s_timer);

  s_catch = ui_box(s_scr);
  lv_obj_set_size(s_catch, 320, 240);
  ui_on_click(s_catch, on_sheet_outside);

  // Mesmo lugar dos balões (ponta na altura da do 1º), mais alta: 162x176.
  constexpr lv_coord_t kX = 150, kY = 18, kTail = 7, kW = 162, kH = 176, kPad = 10;
  constexpr lv_coord_t kInner = kW - 2 - 2 * kPad;  // 140
  constexpr lv_coord_t kBtnW = (kInner - 6) / 2;   // 67
  uint32_t color = n.color;
  s_sheet = ui_box(s_scr);
  lv_obj_set_pos(s_sheet, kX, kY);
  lv_obj_set_size(s_sheet, kW + kTail, kH);
  lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_CLICKABLE);  // toque na folha não fecha

  lv_obj_t *box = ui_box(s_sheet);
  lv_obj_set_pos(box, kTail, 0);
  lv_obj_set_size(box, kW, kH);
  lv_obj_set_style_radius(box, 10, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, c(color), 0);
  lv_obj_set_style_bg_color(box, c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(box, kPad, 0);
  lv_obj_set_style_pad_row(box, 3, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *head = ui_box(box);
  lv_obj_set_size(head, kInner, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(head, 6, 0);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  ui_icon(head, notice_icon(n.icon, true), color);
  ui_label_clamp(ui_label(head, &font_14_700, theme::TEXT, n.title), kInner - 16 - 6, 2);
  if (n.subtitle[0]) ui_label_clamp(ui_label(box, &font_12_600, theme::MUTED, n.subtitle), kInner, 1);
  ui_label_clamp(ui_label(box, &font_12_500, theme::MUTED, n.message[0] ? n.message : tr(STR_SC_ASK_NO_DETAIL)),
                 kInner, 2);
  lv_obj_set_flex_grow(ui_box(box), 1);

  lv_obj_t *row = ui_box(box);
  lv_obj_set_size(row, kInner, 40);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  if (n.actions & NOTICE_ACTION_DENY) {
    lv_obj_t *deny = sheet_button(row, n.actions & NOTICE_ACTION_ALLOW ? kBtnW : kInner);
    lv_obj_set_style_border_width(deny, 1, 0);
    lv_obj_set_style_border_color(deny, c(theme::LINE), 0);
    lv_obj_set_style_bg_color(deny, c(theme::SURFACE_2), 0);
    lv_obj_set_style_bg_opa(deny, LV_OPA_COVER, 0);
    lv_obj_center(ui_label(deny, &font_14_700, theme::TEXT, tr(STR_SC_ASK_DENY)));
    ui_on_click(deny, on_sheet_answer, nullptr);
  }
  if (n.actions & NOTICE_ACTION_ALLOW) {
    // Tracejado e apagado até armar (o LVGL 8 não faz borda tracejada: imagem).
    s_allow = sheet_button(row, n.actions & NOTICE_ACTION_DENY ? kBtnW : kInner);
    if (n.actions & NOTICE_ACTION_DENY) lv_obj_center(ui_icon(s_allow, &icon_dash_btn_67, theme::FAINT_2));
    lv_obj_center(ui_label(s_allow, &font_14_700, theme::FAINT, tr(STR_SC_ASK_ALLOW)));
    ui_on_click(s_allow, on_sheet_answer, (void *)1);
    s_arm = lv_timer_create(on_arm, kArmMs, nullptr);
    lv_timer_set_repeat_count(s_arm, 1);
  }
  lv_obj_t *hint = ui_label(box, &font_12_500, theme::MUTED, tr(STR_SC_ASK_HINT));
  lv_obj_set_width(hint, kInner);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

  // Ponta apontando pra criatura, na mesma altura da do 1º balão.
  constexpr lv_coord_t kTailY = 45 + (50 - 14) / 2 - kY;
  lv_obj_set_pos(ui_icon(s_sheet, &icon_tail_line_8, color), 0, kTailY);
  lv_obj_set_pos(ui_icon(s_sheet, &icon_tail_fill_8, theme::SURFACE), 0, kTailY);
}

struct AskRef {
  Notice n;
  uint32_t rev;
};
AskRef s_asks[3];  // os balões atuais com ações (o toque leva o índice)

void on_ask_tap(lv_event_t *e) {
  const AskRef &a = s_asks[(uintptr_t)lv_event_get_user_data(e)];
  sheet_open(a.n, a.rev);
}

// Indicadores mudaram? A folha aberta ainda vale (o pedido pode ter sido
// respondido no terminal, trocado por outro ou expirado)?
void on_watch(lv_timer_t *) {
  if (statuses().version() != s_chips_version) refresh_chips();
  if (!s_sheet) return;
  int i = notices().find(s_ask_id);
  if (i < 0 || notices().rev((uint8_t)i) != s_ask_rev || millis() - s_ask_opened > kSheetMaxMs) sheet_close();
}

void on_wrap_delete(lv_event_t *e) {
  if (lv_event_get_target(e) == s_wrap) s_wrap = nullptr;
}

void on_delete(lv_event_t *) {
  if (s_timer) lv_timer_del(s_timer);
  s_timer = nullptr;
  if (s_watch) lv_timer_del(s_watch);
  s_watch = nullptr;
  if (s_arm) lv_timer_del(s_arm);
  s_arm = nullptr;
  // Os objetos morrem com a tela.
  s_sheet = s_catch = s_allow = s_wrap = s_chips = s_state_word = nullptr;
  s_armed = false;
  sprite_free(&s_sprite);
  power_use_normal_level();
  s_scr = nullptr;
}

}  // namespace

lv_obj_t *showcase_screen_create() {
  lv_obj_t *scr = ui_screen();
  s_scr = scr;
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);
  ui_on_click(scr, on_exit);
  power_use_showcase_level();

  build_list();
  if (s_count == 0) {
    lv_obj_t *col = ui_box(scr);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_row(col, 6, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_label(col, &font_14_600, theme::MUTED,
             tr(settings().showcase_source == (uint8_t)ShowcaseSource::Caught ? STR_SC_EMPTY_CAUGHT
                                                                              : STR_SC_EMPTY_SEEN));
    ui_label(col, &font_12_500, theme::FAINT, tr(STR_SC_EMPTY_HINT));
    return scr;
  }

  s_card = ui_box(scr);
  lv_obj_set_size(s_card, kCardW, kCardH);
  lv_obj_set_style_pad_ver(s_card, kPadV, 0);
  lv_obj_set_style_pad_hor(s_card, kPadH, 0);

  next();
  s_timer = lv_timer_create(on_tick, (uint32_t)settings().showcase_interval_s * 1000u, nullptr);
  s_watch = lv_timer_create(on_watch, 250, nullptr);
  return scr;
}

// 15b: até 3 balões compactos (162x50, título + subtítulo, sem a mensagem)
// empilhados à direita da janela do sprite; só o 1º tem a ponta. Ficam fora do palco e do painel, então
// sobrevivem às trocas de espécie.
lv_obj_t *showcase_show_notices(const Notice *items, const uint32_t *revs, uint8_t count,
                                lv_event_cb_t on_tap) {
  if (!s_scr) return nullptr;
  constexpr lv_coord_t kTail = 7, kW = 162, kH = 50, kGap = 6, kBalloonPadH = 10;
  constexpr uint8_t kMax = 3;

  // A ponta do 1º encosta na borda direita da janela do sprite (entra 6 px) e
  // o topo da pilha alinha com o topo da janela (sem o deslocamento da troca).
  constexpr lv_coord_t kWinX = kCardX + kPadH, kWinY = kCardY + kPadV + kBodyY + (kBodyH - kWinW) / 2;
  lv_obj_t *wrap = ui_box(s_scr);
  lv_obj_set_pos(wrap, kWinX + kWinW - 6, kWinY);
  lv_obj_set_size(wrap, kW + kTail, kMax * kH + (kMax - 1) * kGap);
  s_wrap = wrap;
  lv_obj_add_event_cb(wrap, on_wrap_delete, LV_EVENT_DELETE, nullptr);
  if (s_sheet) lv_obj_add_flag(wrap, LV_OBJ_FLAG_HIDDEN);  // a folha fica no lugar

  for (uint8_t i = 0; i < count && i < kMax; i++) {
    const Notice &n = items[i];
    uint32_t color = n.color;
    lv_coord_t y = i * (kH + kGap);
    // Pede resposta: "›" à direita e o toque abre a folha (15c).
    bool asks = n.actions != 0;
    constexpr lv_coord_t kChevron = 14 + 4;

    lv_obj_t *b = ui_box(wrap);
    lv_obj_set_pos(b, kTail, y);
    lv_obj_set_size(b, kW, kH);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, c(color), 0);
    lv_obj_set_style_bg_color(b, c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(b, kBalloonPadH, 0);
    lv_obj_set_style_pad_row(b, 2, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *head = ui_box(b);
    lv_obj_set_size(head, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(head, 6, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_coord_t text_w = kW - 2 * kBalloonPadH - 2 - (asks ? kChevron - 2 : 0);
    if (asks) lv_obj_set_style_pad_right(b, 8, 0);
    ui_icon(head, notice_icon(n.icon, true), color);
    // Ícone + título pequeno na cor do aviso; embaixo, em destaque, o
    // subtítulo (a origem) ou, sem ele, a mensagem. Sem os dois, o título
    // fica sozinho em destaque ao lado do ícone.
    const char *second = n.subtitle[0] ? n.subtitle : n.message;
    lv_obj_t *t = second[0] ? ui_label(head, &font_12_700, color, n.title)
                            : ui_label(head, &font_14_700, theme::TEXT, n.title);
    // Uma linha cada, com "…": o LONG_DOT só corta com altura fixa.
    ui_label_clamp(t, text_w - 16 - 6, 1);
    if (second[0]) ui_label_clamp(ui_label(b, &font_14_700, theme::TEXT, second), text_w, 1);
    if (asks) {
      lv_obj_t *chev = ui_icon(b, &icon_forward_14, color);
      lv_obj_add_flag(chev, LV_OBJ_FLAG_IGNORE_LAYOUT);
      lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);
      s_asks[i] = {n, revs[i]};
      ui_on_click(b, on_ask_tap, (void *)(uintptr_t)i);
    } else {
      ui_on_click(b, on_tap, (void *)(uintptr_t)i);
    }

    // Ponta só no 1º (o mais importante): é a criatura falando. Contorno na
    // cor do aviso, miolo surface por cima (cobre a borda).
    if (i > 0) continue;
    lv_obj_t *line = ui_icon(wrap, &icon_tail_line_8, color);
    lv_obj_set_pos(line, 0, y + (kH - 14) / 2);
    lv_obj_t *fill = ui_icon(wrap, &icon_tail_fill_8, theme::SURFACE);
    lv_obj_set_pos(fill, 0, y + (kH - 14) / 2);
  }
  return wrap;
}
