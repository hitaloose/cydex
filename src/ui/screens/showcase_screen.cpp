// 14 Modo vitrine (canvas: Vitrine.dc.html, "carta"; CLAUDE.md "Modo vitrine").
//   cartão 314x234 sem moldura (3 px da borda da tela, padding 3/5):
//     cabeçalho 22: nome (16 px, 1 linha, "…") e tipos abreviados à direita
//     corpo: janela 148x148 (borda 2 na cor do tipo 1 a 50% sobre o fundo) com o sprite 96 em 2x
//       cortado em 144 (72 px do original, centrados na parte opaca; mais alto
//       que 72, ancora embaixo) · medidas + 6 status com rótulo, valor e barra
//     rodapé 18: "#nº · Geração X" e o status de captura
//   Mostrar "Todos" inclui as não vistas: só nº, nome e sprite; tipos, medidas
//     e valores viram "???", borda neutra (canvas: tweak naoVisto)
//   tocar em qualquer lugar volta pro Menu
// Só redesenha na troca (timer do intervalo dos Ajustes); entre trocas nada
// muda. Cada troca desloca tudo 1–2 px (anti-retenção). Brilho próprio
// enquanto a tela existe; a tela nunca apaga aqui (ui/power).
#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "data/Settings.h"
#include "data/SpeciesRepo.h"
#include "data/SpriteLoader.h"
#include "data/StatusStore.h"
#include "dex/Dex.h"
#include "i18n/i18n.h"
#include "logic/Format.h"
#include "logic/Showcase.h"
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
constexpr lv_coord_t kColX = kWinW + 8, kColW = kInnerW - kColX;  // 148
constexpr lv_coord_t kStatH = 14, kStatGap = 3, kStatLabelW = 26, kStatValueW = 24;
constexpr lv_coord_t kStatBarX = kStatLabelW + 4 + kStatValueW + 4, kStatBarH = 4;
constexpr lv_coord_t kChipPad = 7, kChipGap = 4;
constexpr uint16_t kChromaKey = 0x07E0;  // lv_color_hex(0x00FF00) em RGB565 (tools/build_sd.py)

uint16_t s_list[DEX_COUNT];  // espécies elegíveis (2 KB)
uint16_t s_count;
int32_t s_pos = -1;      // sobrevive entre sessões: a sequencial continua de onde parou
uint32_t s_shift_k;
Sprite s_sprite;
lv_obj_t *s_card;
lv_timer_t *s_timer;
lv_obj_t *s_scr;

const StrId kStatLabels[SPECIES_STAT_COUNT] = {STR_STAT_HP,  STR_STAT_ATK, STR_STAT_DEF,
                                               STR_STAT_SPA, STR_STAT_SPD, STR_STAT_SPE};

void build_list() {
  const DexStatus &st = status_store_get();
  ShowcaseSource src = (ShowcaseSource)settings().showcase_source;
  s_count = 0;
  for (uint16_t n = 1; n <= species_repo_count(); n++) {
    if (showcase_eligible(src, st.is_seen(n), st.is_caught(n))) s_list[s_count++] = n;
  }
}

lv_coord_t chip_w(const char *text) {
  return lv_txt_get_width(text, strlen(text), &font_12_700, 0, LV_TEXT_FLAG_NONE) + 2 * kChipPad;
}

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

  lv_obj_t *nl = ui_label(s_card, &font_16_700, theme::TEXT, species_repo_name(n));
  ui_label_clamp(nl, kInnerW - used - (used ? 8 : 0), 1);
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
  lv_obj_set_size(win, kWinW, kWinW);
  lv_obj_set_pos(win, 0, kBodyY + (kBodyH - kWinW) / 2);
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

  lv_obj_t *id = ui_label(foot, &font_12_600, theme::MUTED, "");
  lv_label_set_text_fmt(id, tr(STR_SC_FOOTER_FMT), n, dex_generation_roman(dex_generation_of(n)));
  lv_obj_set_flex_grow(id, 1);

  bool caught = state == SpeciesState::Caught;
  bool unseen = state == SpeciesState::Unknown;
  ui_status_mark_set(ui_status_mark(foot, 12), state);
  ui_label(foot, caught ? &font_12_700 : &font_12_600,
           caught ? theme::ACCENT : unseen ? theme::MUTED : theme::TEXT,
           tr(caught ? STR_STATE_CAUGHT : unseen ? STR_STATE_UNSEEN : STR_STATE_SEEN));
}

void show(uint16_t n) {
  uint32_t t0 = lv_tick_get();
  // O cartão sai antes do sprite: o lv_img ainda aponta pro buffer.
  lv_obj_clean(s_card);
  sprite_free(&s_sprite);

  SpeciesRecord rec;
  bool have = species_repo_get(n, &rec);
  SpeciesState state = status_store_get().state(n);
  bool unseen = state == SpeciesState::Unknown;  // só com Mostrar "Todos"
  int8_t dx, dy;
  showcase_shift(s_shift_k++, &dx, &dy);
  lv_obj_set_pos(s_card, kCardX + dx, kCardY + dy);

  const SpeciesRecord *r = have ? &rec : nullptr;
  build_header(n, r, unseen);
  build_window(n, r, unseen);
  build_stats(r, unseen);
  build_footer(n, state);

  LOG_D("showcase", "#%u em %ums, maior_bloco=%u", n, lv_tick_elaps(t0),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void next() {
  s_pos = showcase_next(s_pos < s_count ? s_pos : -1, s_count, settings().showcase_random,
                        esp_random());
  show(s_list[s_pos]);
}

void on_tick(lv_timer_t *) { next(); }

void on_exit(lv_event_t *) { nav_go(Screen::Menu); }

void on_delete(lv_event_t *) {
  if (s_timer) lv_timer_del(s_timer);
  s_timer = nullptr;
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
  return scr;
}

// 15b: até 3 balões compactos (162x50, título + subtítulo, sem a mensagem)
// empilhados à direita da janela do sprite; só o 1º tem a ponta. Ficam fora do palco e do painel, então
// sobrevivem às trocas de espécie.
lv_obj_t *showcase_show_notices(const Notice *items, uint8_t count, lv_event_cb_t on_tap) {
  if (!s_scr) return nullptr;
  constexpr lv_coord_t kTail = 7, kW = 162, kH = 50, kGap = 6, kBalloonPadH = 10;
  constexpr uint8_t kMax = 3;

  // A ponta do 1º encosta na borda direita da janela do sprite (entra 6 px) e
  // o topo da pilha alinha com o topo da janela (sem o deslocamento da troca).
  constexpr lv_coord_t kWinX = kCardX + kPadH, kWinY = kCardY + kPadV + kBodyY + (kBodyH - kWinW) / 2;
  lv_obj_t *wrap = ui_box(s_scr);
  lv_obj_set_pos(wrap, kWinX + kWinW - 6, kWinY);
  lv_obj_set_size(wrap, kW + kTail, kMax * kH + (kMax - 1) * kGap);

  for (uint8_t i = 0; i < count && i < kMax; i++) {
    const Notice &n = items[i];
    uint32_t color = n.color;
    lv_coord_t y = i * (kH + kGap);

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
    constexpr lv_coord_t kTextW = kW - 2 * kBalloonPadH - 2;
    ui_icon(head, notice_icon(n.icon, true), color);
    // Ícone + título pequeno na cor do aviso; embaixo, em destaque, o
    // subtítulo (a origem) ou, sem ele, a mensagem. Sem os dois, o título
    // fica sozinho em destaque ao lado do ícone.
    const char *second = n.subtitle[0] ? n.subtitle : n.message;
    lv_obj_t *t = second[0] ? ui_label(head, &font_12_700, color, n.title)
                            : ui_label(head, &font_14_700, theme::TEXT, n.title);
    lv_obj_set_width(t, kTextW - 16 - 6);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    if (second[0]) {
      lv_obj_t *l = ui_label(b, &font_14_700, theme::TEXT, second);
      lv_obj_set_width(l, kTextW);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }
    ui_on_click(b, on_tap, (void *)(uintptr_t)i);

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
