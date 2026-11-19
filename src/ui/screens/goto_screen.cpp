// 06b Ir para (canvas: IrPara.dc.html).
//   esquerda: 9 gerações (3x3) -> volta pra Lista/Grade na 1a espécie da geração
//   direita: campo "#___" + teclado numérico; "Ir" abre a ficha do número
#include <cstdio>

#include "data/SpeciesRepo.h"
#include "dex/Dex.h"
#include "i18n/i18n.h"
#include "logic/NumberInput.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/icons/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

constexpr lv_coord_t kPad = 6, kColGap = 8, kLeftW = 150;
constexpr lv_coord_t kTop = theme::HEADER_H + kPad;
constexpr lv_coord_t kCaptionH = 16, kCaptionGap = 4;
constexpr lv_coord_t kBodyH = 240 - kTop - kPad;  // 196

NumberInput s_input;
lv_obj_t *s_display;

void refresh_display() {
  char buf[8];
  s_input.format(buf, sizeof(buf));
  lv_label_set_text(s_display, buf);
}

void on_back(lv_event_t *) { nav_go(catalog_state().view); }

void on_gen(lv_event_t *e) {
  uint8_t g = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  catalog_state().anchor = DEX_GEN_START[g];
  nav_go(catalog_state().view);
}

void on_digit(lv_event_t *e) {
  if (s_input.press_digit((uint8_t)(uintptr_t)lv_event_get_user_data(e))) refresh_display();
}

void on_backspace(lv_event_t *) {
  s_input.backspace();
  refresh_display();
}

void on_go(lv_event_t *) {
  if (s_input.empty()) return;
  // Como a geração: abre a página que contém o número (ou o próximo que
  // passe no filtro), na visão de origem.
  CatalogState &cs = catalog_state();
  cs.anchor = s_input.value;
  nav_go(cs.view);
}

lv_obj_t *caption(lv_obj_t *scr, lv_coord_t x, StrId text) {
  lv_obj_t *l = ui_label(scr, &font_12_600, theme::MUTED, tr(text));
  lv_obj_set_pos(l, x, kTop);
  return l;
}

lv_obj_t *key(lv_obj_t *scr, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, uint32_t bg) {
  lv_obj_t *k = ui_box(scr);
  lv_obj_set_pos(k, x, y);
  lv_obj_set_size(k, w, h);
  lv_obj_set_style_radius(k, 4, 0);
  lv_obj_set_style_bg_color(k, c(bg), 0);
  lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(k, c(theme::LINE), LV_STATE_PRESSED);
  return k;
}

}  // namespace

lv_obj_t *goto_screen_create() {
  s_input = NumberInput();
  s_input.max = species_repo_count();
  lv_obj_t *scr = ui_screen();
  ui_header(scr, tr(STR_TITLE_GOTO), on_back);

  // ---- Gerações (3x3) ----
  caption(scr, kPad, STR_GOTO_GENERATION);
  const lv_coord_t gy = kTop + kCaptionH + kCaptionGap;
  const lv_coord_t gh = kBodyH - kCaptionH - kCaptionGap;  // 176
  const lv_coord_t gw = (kLeftW - 2 * 3) / 3;               // 48
  const lv_coord_t gcell_h = (gh - 2 * 3) / 3;              // 56
  for (uint8_t g = 0; g < DEX_GENERATIONS; g++) {
    lv_obj_t *b = key(scr, kPad + (g % 3) * (gw + 3), gy + (g / 3) * (gcell_h + 3), gw, gcell_h,
                      theme::SURFACE);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, c(theme::LINE), 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, 1, 0);
    ui_label(b, &font_14_700, theme::TEXT, dex_generation_roman(g + 1));
    char from[8];
    snprintf(from, sizeof(from), "%03u", DEX_GEN_START[g]);
    ui_label(b, &font_12_500, theme::MUTED, from);
    ui_on_click(b, on_gen, (void *)(uintptr_t)g);
  }

  // ---- Número ----
  const lv_coord_t rx = kPad + kLeftW + kColGap;
  const lv_coord_t rw = 320 - rx - kPad;  // 150
  caption(scr, rx, STR_GOTO_NUMBER);

  lv_obj_t *field = ui_box(scr);
  const lv_coord_t fy = kTop + kCaptionH + kCaptionGap;
  lv_obj_set_pos(field, rx, fy);
  lv_obj_set_size(field, rw, 30);
  lv_obj_set_style_radius(field, 6, 0);
  lv_obj_set_style_border_width(field, 1, 0);
  lv_obj_set_style_border_color(field, c(theme::ACCENT), 0);
  lv_obj_set_style_pad_hor(field, 10, 0);
  s_display = ui_label(field, &font_16_700, theme::TEXT, "");
  lv_obj_align(s_display, LV_ALIGN_LEFT_MID, 0, 0);
  refresh_display();

  const lv_coord_t ky = fy + 30 + kCaptionGap;
  const lv_coord_t kw = (rw - 2 * 3) / 3;                       // 48
  const lv_coord_t kh = (240 - kPad - ky - 3 * 3) / 4;          // 33
  auto pos = [&](int i, lv_coord_t *x, lv_coord_t *y) {
    *x = rx + (i % 3) * (kw + 3);
    *y = ky + (i / 3) * (kh + 3);
  };
  for (int d = 1; d <= 9; d++) {
    lv_coord_t x, y;
    pos(d - 1, &x, &y);
    lv_obj_t *k = key(scr, x, y, kw, kh, theme::SURFACE_2);
    char t[2] = {(char)('0' + d), 0};
    lv_obj_center(ui_label(k, &font_14_700, theme::TEXT, t));
    ui_on_click(k, on_digit, (void *)(uintptr_t)d);
  }
  lv_coord_t x, y;
  pos(9, &x, &y);
  lv_obj_t *bs = key(scr, x, y, kw, kh, theme::LINE);
  lv_obj_set_style_bg_color(bs, c(theme::FAINT_2), LV_STATE_PRESSED);
  lv_obj_center(ui_icon(bs, &icon_backspace_18, theme::TEXT));
  ui_on_click(bs, on_backspace);

  pos(10, &x, &y);
  lv_obj_t *zero = key(scr, x, y, kw, kh, theme::SURFACE_2);
  lv_obj_center(ui_label(zero, &font_14_700, theme::TEXT, "0"));
  ui_on_click(zero, on_digit, (void *)(uintptr_t)0);

  pos(11, &x, &y);
  lv_obj_t *go = key(scr, x, y, kw, kh, theme::ACCENT);
  lv_obj_set_style_bg_color(go, c(theme::ACCENT), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(go, LV_OPA_80, LV_STATE_PRESSED);
  lv_obj_center(ui_label(go, &font_14_700, theme::ON_ACCENT, tr(STR_GOTO_GO)));
  ui_on_click(go, on_go);

  return scr;
}
