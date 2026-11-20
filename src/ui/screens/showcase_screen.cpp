// 14 Modo vitrine (canvas: Vitrine.dc.html; CLAUDE.md "Modo vitrine").
//   palco 164x192 à esquerda: sombra no chão (cor do tipo 1) + sprite 96 em até
//     2x, reduzido pra parte opaca caber nos 164 px (6 sprites usam os 96 px
//     inteiros), com os pés no fundo do palco
//   painel de 144 px à direita: #nº, nome (1 linha, "…"), tipos lado a lado,
//     medidas, status, mini status
//   Mostrar "Todos" inclui as não vistas: só nº, nome e sprite; tipos, medidas
//     e status viram "???" (canvas: tweak naoVisto)
//   tocar em qualquer lugar volta pro Menu
// Só redesenha na troca (timer do intervalo dos Ajustes); entre trocas nada
// muda. Cada troca desloca tudo 1–2 px (anti-retenção). Brilho próprio
// enquanto a tela existe; a tela nunca apaga aqui (ui/power).
#include <esp_random.h>

#include <cstdio>

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

constexpr lv_coord_t kStageX = 2, kStageY = 24, kStageW = 164, kStageH = 192;
constexpr lv_coord_t kPanelX = 172, kPanelY = 18, kPanelW = 144, kPanelH = 204;
constexpr lv_coord_t kFloorW = 140, kFloorH = 22;
constexpr uint16_t kChromaKey = 0x07E0;  // lv_color_hex(0x00FF00) em RGB565 (tools/build_sd.py)

uint16_t s_list[DEX_COUNT];  // espécies elegíveis (2 KB)
uint16_t s_count;
int32_t s_pos = -1;      // sobrevive entre sessões: a sequencial continua de onde parou
uint32_t s_shift_k;
Sprite s_sprite;
lv_obj_t *s_stage;
lv_obj_t *s_panel;
lv_timer_t *s_timer;
lv_obj_t *s_scr;

void build_list() {
  const DexStatus &st = status_store_get();
  ShowcaseSource src = (ShowcaseSource)settings().showcase_source;
  s_count = 0;
  for (uint16_t n = 1; n <= species_repo_count(); n++) {
    if (showcase_eligible(src, st.is_seen(n), st.is_caught(n))) s_list[s_count++] = n;
  }
}

lv_obj_t *spaced(lv_obj_t *obj, lv_coord_t top) {
  lv_obj_set_style_pad_top(obj, top, 0);  // "margin-top" do canvas
  return obj;
}

void show(uint16_t n) {
  uint32_t t0 = lv_tick_get();
  sprite_free(&s_sprite);
  lv_obj_clean(s_stage);
  lv_obj_clean(s_panel);

  SpeciesRecord rec;
  bool have = species_repo_get(n, &rec);
  SpeciesState state = status_store_get().state(n);
  bool unseen = state == SpeciesState::Unknown;  // só com Mostrar "Todos"
  int8_t dx, dy;
  showcase_shift(s_shift_k++, &dx, &dy);
  lv_obj_set_pos(s_stage, kStageX + dx, kStageY + dy);
  lv_obj_set_pos(s_panel, kPanelX + dx, kPanelY + dy);

  // Sombra no chão: elipse 140x22 de borda suave (ícone A8, o LVGL não faz
  // elipse) na cor do tipo 1, a ~18%.
  SpeciesType t1 = have ? rec.type1 : SpeciesType::None;
  if (t1 != SpeciesType::None) {
    // Não vista: sombra neutra (a cor entregaria o tipo).
    uint32_t tint = unseen ? theme::FAINT : species_type_color(t1);
    lv_obj_t *floor = ui_icon(s_stage, &icon_floor_140, tint);
    lv_obj_set_pos(floor, (kStageW - kFloorW) / 2, kStageH - 14 - kFloorH);
    lv_obj_set_style_img_opa(floor, 46, 0);
  }
  if (sprite_load(n, SpriteSize::S96, &s_sprite)) {
    // Até 2x, vizinho mais próximo. Os mais largos não cabem em 2x no palco e
    // descem até ~1,7x. O centro da parte opaca vai pro meio do palco e o pé do
    // quadro fica no fundo (os pés não sobem quando a escala diminui).
    const uint16_t side = s_sprite.dsc.header.w;
    uint8_t x0 = 0, x1 = side - 1;
    showcase_opaque_cols((const uint16_t *)s_sprite.dsc.data, side, kChromaKey, &x0, &x1);
    uint16_t zoom = showcase_fit_zoom(x1 - x0 + 1, kStageW);
    lv_coord_t off = (lv_coord_t)(((int32_t)side - (x0 + x1 + 1)) * zoom / 512);
    lv_obj_t *img = lv_img_create(s_stage);
    lv_img_set_src(img, &s_sprite.dsc);
    lv_img_set_antialias(img, false);
    lv_img_set_zoom(img, zoom);
    lv_img_set_size_mode(img, LV_IMG_SIZE_MODE_REAL);
    lv_obj_align(img, LV_ALIGN_BOTTOM_MID, off, 0);
  }

  lv_obj_t *num = ui_label(s_panel, &font_14_600, theme::MUTED, "");
  lv_label_set_text_fmt(num, "#%03u", n);
  const char *name = species_repo_name(n);
  lv_obj_t *nl = ui_label(s_panel, &font_20_700, theme::TEXT, name);
  ui_label_clamp(nl, kPanelW, 1);

  lv_obj_t *types = spaced(ui_box(s_panel), 2);
  lv_obj_set_size(types, kPanelW, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(types, 4, 0);
  lv_obj_set_style_pad_column(types, 4, 0);
  lv_obj_set_flex_flow(types, LV_FLEX_FLOW_ROW_WRAP);  // desce só se a dupla não couber
  if (unseen) {
    lv_obj_t *chip = ui_chip(types, tr(STR_UNKNOWN_NAME), theme::BG, theme::MUTED);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, c(theme::FAINT), 0);
  } else if (have) {
    for (SpeciesType t : {rec.type1, rec.type2}) {
      if (t != SpeciesType::None)
        ui_chip(types, species_type_name(t), species_type_color(t), theme::ON_ACCENT);
    }
  }

  if (unseen) {
    char line[44];
    snprintf(line, sizeof(line), "%s m · %s kg", tr(STR_UNKNOWN_NAME), tr(STR_UNKNOWN_NAME));
    spaced(ui_label(s_panel, &font_12_500, theme::MUTED, line), 6);
  } else if (have) {
    char h[12], w[12], hs[20], ws[20], line[44];
    format_tenths(rec.height_dm, tr(STR_DECIMAL_SEP), h, sizeof(h));
    format_tenths(rec.weight_hg, tr(STR_DECIMAL_SEP), w, sizeof(w));
    snprintf(hs, sizeof(hs), tr(STR_DETAIL_HEIGHT_FMT), h);
    snprintf(ws, sizeof(ws), tr(STR_DETAIL_WEIGHT_FMT), w);
    snprintf(line, sizeof(line), "%s · %s", hs, ws);
    spaced(ui_label(s_panel, &font_12_500, theme::MUTED, line), 6);
  }

  bool caught = state == SpeciesState::Caught;
  lv_obj_t *st = spaced(ui_box(s_panel), 2);
  lv_obj_set_size(st, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(st, 5, 0);
  lv_obj_set_flex_flow(st, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(st, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_status_mark_set(ui_status_mark(st, 12), state);
  ui_label(st, caught ? &font_12_700 : &font_12_600,
           caught ? theme::ACCENT : unseen ? theme::MUTED : theme::TEXT,
           tr(caught ? STR_STATE_CAUGHT : unseen ? STR_STATE_UNSEEN : STR_STATE_SEEN));

  lv_obj_t *grow = ui_box(s_panel);
  lv_obj_set_width(grow, 1);
  lv_obj_set_flex_grow(grow, 1);

  // Mini status: PS..Vel sem rótulo, escala 150, >= 60 em accent.
  lv_obj_t *total = ui_label(s_panel, &font_12_600, theme::MUTED, "");
  if (unseen) lv_label_set_text(total, tr(STR_SC_TOTAL_UNKNOWN));
  else lv_label_set_text_fmt(total, tr(STR_SC_TOTAL_FMT), have ? rec.stat_total() : 0);
  lv_obj_t *bars = spaced(ui_box(s_panel), 5);
  lv_obj_set_size(bars, kPanelW, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(bars, 4, 0);
  lv_obj_set_flex_flow(bars, LV_FLEX_FLOW_COLUMN);
  for (int i = 0; i < SPECIES_STAT_COUNT; i++) {
    uint8_t v = have && !unseen ? rec.stats[i] : 0;  // não vista: trilhos vazios
    lv_obj_t *bar = ui_progress(bars, kPanelW, 5);
    lv_obj_set_style_bg_color(bar, c(v >= 60 ? theme::ACCENT : theme::TEXT), LV_PART_INDICATOR);
    ui_progress_set(bar, v > 150 ? 150 : v, 150);
  }

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
  power_set_level(settings().showcase_brightness);

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

  s_stage = ui_box(scr);
  lv_obj_set_size(s_stage, kStageW, kStageH);
  s_panel = ui_box(scr);
  lv_obj_set_size(s_panel, kPanelW, kPanelH);
  lv_obj_set_flex_flow(s_panel, LV_FLEX_FLOW_COLUMN);

  next();
  s_timer = lv_timer_create(on_tick, (uint32_t)settings().showcase_interval_s * 1000u, nullptr);
  return scr;
}

// 15b: até 3 balões compactos (162x50, tipo + projeto, sem a mensagem), como
// se a criatura falasse várias coisas. Ficam fora do palco e do painel, então
// sobrevivem às trocas de espécie.
lv_obj_t *showcase_show_notices(const Notice *items, uint8_t count, lv_event_cb_t on_tap) {
  if (!s_scr) return nullptr;
  constexpr lv_coord_t kTail = 7, kW = 162, kH = 50, kGap = 6, kPadH = 10;
  constexpr uint8_t kMax = 3;

  lv_obj_t *wrap = ui_box(s_scr);
  lv_obj_set_pos(wrap, 150 - kTail, 14);
  lv_obj_set_size(wrap, kW + kTail, kMax * kH + (kMax - 1) * kGap);

  for (uint8_t i = 0; i < count && i < kMax; i++) {
    const Notice &n = items[i];
    uint32_t color = notice_color(n.kind);
    lv_coord_t y = i * (kH + kGap);

    lv_obj_t *b = ui_box(wrap);
    lv_obj_set_pos(b, kTail, y);
    lv_obj_set_size(b, kW, kH);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, c(color), 0);
    lv_obj_set_style_bg_color(b, c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(b, kPadH, 0);
    lv_obj_set_style_pad_row(b, 2, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *head = ui_box(b);
    lv_obj_set_size(head, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(head, 6, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_icon(head, notice_icon(n.kind, true), color);
    StrId kind = n.kind == NoticeKind::Permission ? STR_NOTICE_PERMISSION_SHORT
                 : n.kind == NoticeKind::Idle     ? STR_NOTICE_IDLE_SHORT
                                                  : STR_NOTICE_DONE_SHORT;
    ui_label(head, &font_12_700, color, tr(kind));

    lv_obj_t *proj = ui_label(b, &font_14_700, theme::TEXT, n.project);
    lv_obj_set_width(proj, kW - 2 * kPadH - 2);
    lv_label_set_long_mode(proj, LV_LABEL_LONG_DOT);
    ui_on_click(b, on_tap, (void *)(uintptr_t)i);

    // Ponta: contorno na cor do aviso, miolo surface por cima (cobre a borda).
    lv_obj_t *line = ui_icon(wrap, &icon_tail_line_8, color);
    lv_obj_set_pos(line, 0, y + (kH - 14) / 2);
    lv_obj_t *fill = ui_icon(wrap, &icon_tail_fill_8, theme::SURFACE);
    lv_obj_set_pos(fill, 0, y + (kH - 14) / 2);
  }
  return wrap;
}
