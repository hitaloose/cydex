// 14 Modo vitrine (canvas: Vitrine.dc.html; CLAUDE.md "Modo vitrine").
//   palco 192x192 à esquerda: sombra no chão (cor do tipo 1) + sprite 96 em 2x
//   painel de 104 px à direita: #nº, nome, tipos, medidas, status, mini status
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
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/power.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "util/log.h"

namespace {

constexpr lv_coord_t kStageX = 6, kStageY = 24, kStage = 192;
constexpr lv_coord_t kPanelX = 208, kPanelY = 18, kPanelW = 104, kPanelH = 204;

uint16_t s_list[DEX_COUNT];  // espécies elegíveis (2 KB)
uint16_t s_count;
int32_t s_pos = -1;      // sobrevive entre sessões: a sequencial continua de onde parou
uint32_t s_shift_k;
Sprite s_sprite;
lv_obj_t *s_stage;
lv_obj_t *s_panel;
lv_timer_t *s_timer;

void build_list() {
  const DexStatus &st = status_store_get();
  bool all_seen = settings().showcase_all_seen;
  s_count = 0;
  for (uint16_t n = 1; n <= species_repo_count(); n++) {
    if (all_seen ? st.is_seen(n) : st.is_caught(n)) s_list[s_count++] = n;
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
  int8_t dx, dy;
  showcase_shift(s_shift_k++, &dx, &dy);
  lv_obj_set_pos(s_stage, kStageX + dx, kStageY + dy);
  lv_obj_set_pos(s_panel, kPanelX + dx, kPanelY + dy);

  // Sombra no chão: elipse 140x22 de borda suave (ícone A8, o LVGL não faz
  // elipse) na cor do tipo 1, a ~18%.
  SpeciesType t1 = have ? rec.type1 : SpeciesType::None;
  if (t1 != SpeciesType::None) {
    lv_obj_t *floor = ui_icon(s_stage, &icon_floor_140, species_type_color(t1));
    lv_obj_set_pos(floor, 26, kStage - 14 - 22);
    lv_obj_set_style_img_opa(floor, 46, 0);
  }
  if (sprite_load(n, SpriteSize::S96, &s_sprite)) {
    // 2x com vizinho mais próximo: pixel art nítida e escala inteira.
    lv_obj_t *img = lv_img_create(s_stage);
    lv_img_set_src(img, &s_sprite.dsc);
    lv_img_set_antialias(img, false);
    lv_img_set_zoom(img, 512);
    lv_img_set_size_mode(img, LV_IMG_SIZE_MODE_REAL);
    lv_obj_center(img);
  }

  lv_obj_t *num = ui_label(s_panel, &font_14_600, theme::MUTED, "");
  lv_label_set_text_fmt(num, "#%03u", n);
  const char *name = species_repo_name(n);
  lv_obj_t *nl = ui_label(s_panel, strlen(name) > 9 ? &font_16_700 : &font_20_700, theme::TEXT, name);
  lv_obj_set_width(nl, kPanelW);
  lv_label_set_long_mode(nl, LV_LABEL_LONG_DOT);

  lv_obj_t *types = spaced(ui_box(s_panel), 2);
  lv_obj_set_size(types, kPanelW, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(types, 4, 0);
  lv_obj_set_flex_flow(types, LV_FLEX_FLOW_COLUMN);
  if (have) {
    for (SpeciesType t : {rec.type1, rec.type2}) {
      if (t != SpeciesType::None)
        ui_chip(types, species_type_name(t), species_type_color(t), theme::ON_ACCENT);
    }
  }

  if (have) {
    char h[12], w[12], hs[20], ws[20], line[44];
    format_tenths(rec.height_dm, tr(STR_DECIMAL_SEP), h, sizeof(h));
    format_tenths(rec.weight_hg, tr(STR_DECIMAL_SEP), w, sizeof(w));
    snprintf(hs, sizeof(hs), tr(STR_DETAIL_HEIGHT_FMT), h);
    snprintf(ws, sizeof(ws), tr(STR_DETAIL_WEIGHT_FMT), w);
    snprintf(line, sizeof(line), "%s · %s", hs, ws);
    spaced(ui_label(s_panel, &font_12_500, theme::MUTED, line), 6);
  }

  SpeciesState state = status_store_get().state(n);
  bool caught = state == SpeciesState::Caught;
  lv_obj_t *st = spaced(ui_box(s_panel), 2);
  lv_obj_set_size(st, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(st, 5, 0);
  lv_obj_set_flex_flow(st, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(st, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_status_mark_set(ui_status_mark(st, 12), state);
  ui_label(st, &font_12_700, caught ? theme::ACCENT : theme::TEXT,
           tr(caught ? STR_STATE_CAUGHT : STR_STATE_SEEN));

  lv_obj_t *grow = ui_box(s_panel);
  lv_obj_set_width(grow, 1);
  lv_obj_set_flex_grow(grow, 1);

  // Mini status: PS..Vel sem rótulo, escala 150, >= 60 em accent.
  lv_obj_t *total = ui_label(s_panel, &font_12_600, theme::MUTED, "");
  lv_label_set_text_fmt(total, tr(STR_SC_TOTAL_FMT), have ? rec.stat_total() : 0);
  lv_obj_t *bars = spaced(ui_box(s_panel), 5);
  lv_obj_set_size(bars, kPanelW, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(bars, 4, 0);
  lv_obj_set_flex_flow(bars, LV_FLEX_FLOW_COLUMN);
  for (int i = 0; i < SPECIES_STAT_COUNT; i++) {
    uint8_t v = have ? rec.stats[i] : 0;
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
}

}  // namespace

lv_obj_t *showcase_screen_create() {
  lv_obj_t *scr = ui_screen();
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
             tr(settings().showcase_all_seen ? STR_SC_EMPTY_SEEN : STR_SC_EMPTY_CAUGHT));
    ui_label(col, &font_12_500, theme::FAINT, tr(STR_SC_EMPTY_HINT));
    return scr;
  }

  s_stage = ui_box(scr);
  lv_obj_set_size(s_stage, kStage, kStage);
  s_panel = ui_box(scr);
  lv_obj_set_size(s_panel, kPanelW, kPanelH);
  lv_obj_set_flex_flow(s_panel, LV_FLEX_FLOW_COLUMN);

  next();
  s_timer = lv_timer_create(on_tick, (uint32_t)settings().showcase_interval_s * 1000u, nullptr);
  return scr;
}
