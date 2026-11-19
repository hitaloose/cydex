// 04 Progresso (canvas: Progresso.dc.html).
//   cabeçalho: voltar, "Progresso", "Nacional" à direita
//   esquerda: anel 96 px de % capturados + capturados/vistos
//   direita: 9 barras por geração (capturados / total da geração)
// Tudo sai do DexStatus em RAM; nada é lido do SD.
#include "data/StatusStore.h"
#include "dex/Dex.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kPadX = 10, kTop = theme::HEADER_H + 10, kBottom = 240 - 8;
constexpr lv_coord_t kLeftW = 100, kColGap = 12;
constexpr lv_coord_t kListX = kPadX + kLeftW + kColGap;  // 122
constexpr lv_coord_t kListW = 320 - kListX - kPadX;      // 188
constexpr lv_coord_t kGenRowH = 18;

void on_back(lv_event_t *) { nav_go(Screen::Menu); }

void count_row(lv_obj_t *parent, lv_coord_t y, SpeciesState state, uint16_t value, StrId label) {
  lv_obj_t *row = ui_box(parent);
  lv_obj_set_pos(row, 0, y);
  lv_obj_set_size(row, kLeftW, 18);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ui_status_mark_set(ui_status_mark(row), state);
  lv_obj_t *v = ui_label(row, &font_14_700, theme::TEXT, "");
  lv_label_set_text_fmt(v, "%u", value);
  ui_label(row, &font_12_500, theme::MUTED, tr(label));
}

}  // namespace

lv_obj_t *progress_screen_create() {
  const DexStatus &st = status_store_get();
  uint16_t caught = st.count_caught();
  uint16_t seen = st.count_seen();

  lv_obj_t *scr = ui_screen();
  lv_obj_t *h = ui_header(scr, tr(STR_TITLE_PROGRESS), on_back);
  lv_obj_set_style_pad_right(h, 10, 0);
  ui_header_spacer(h);
  ui_label(h, &font_12_600, theme::MUTED, tr(STR_CATALOG_NATIONAL));

  // Coluna da esquerda.
  lv_obj_t *left = ui_box(scr);
  lv_obj_set_pos(left, kPadX, kTop);
  lv_obj_set_size(left, kLeftW, kBottom - kTop);

  // Anel: r=40, traço 10 (SVG do canvas) = arco de 90 px com 10 de largura
  // dentro de uma caixa de 96 (a ponta arredondada encosta na borda).
  lv_obj_t *arc = lv_arc_create(left);
  lv_obj_remove_style_all(arc);
  lv_obj_set_size(arc, 90, 90);
  lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 3);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, c(theme::SURFACE_2), LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, c(theme::ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_arc_set_rotation(arc, 270);  // começa no topo
  lv_arc_set_bg_angles(arc, 0, 360);
  lv_arc_set_range(arc, 0, DEX_COUNT);
  lv_arc_set_value(arc, caught);
  if (caught == 0) lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);

  lv_obj_t *center = ui_box(left);
  lv_obj_set_size(center, 96, 96);
  lv_obj_align(center, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_flex_flow(center, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *pct = ui_label(center, &font_20_700, theme::TEXT, "");
  lv_label_set_text_fmt(pct, "%u%%", (unsigned)(caught * 100u / DEX_COUNT));
  ui_label(center, &font_12_500, theme::MUTED, tr(STR_PROGRESS_CAUGHT_LABEL));

  count_row(left, 96 + 10, SpeciesState::Caught, caught, STR_PROGRESS_CAUGHT_SHORT);
  count_row(left, 96 + 10 + 18 + 6, SpeciesState::Seen, seen, STR_PROGRESS_SEEN);

  // Barras por geração, distribuídas na altura (justify space-between).
  constexpr lv_coord_t kLabelW = 30, kValueW = 52, kGap = 6;
  constexpr lv_coord_t kBarW = kListW - kLabelW - kValueW - 2 * kGap;
  const lv_coord_t step = (kBottom - kTop - kGenRowH) / (DEX_GENERATIONS - 1);
  for (uint8_t g = 0; g < DEX_GENERATIONS; g++) {
    uint16_t first = DEX_GEN_START[g], last = DEX_GEN_START[g + 1] - 1;
    uint16_t got = st.count_caught_range(first, last);
    uint16_t total = last - first + 1;
    lv_coord_t y = kTop + g * step;

    lv_obj_t *l = ui_label(scr, &font_12_700, theme::MUTED, dex_generation_roman(g + 1));
    lv_obj_set_pos(l, kListX, y + 1);
    lv_obj_t *bar = ui_progress(scr, kBarW, 6);
    lv_obj_set_pos(bar, kListX + kLabelW + kGap, y + (kGenRowH - 6) / 2);
    ui_progress_set(bar, got, total);
    lv_obj_t *v = ui_label(scr, &font_12_600, theme::TEXT, "");
    lv_label_set_text_fmt(v, "%u/%u", got, total);
    lv_obj_set_width(v, kValueW);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(v, kListX + kListW - kValueW, y + 1);
  }
  return scr;
}
