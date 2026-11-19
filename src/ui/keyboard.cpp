#include "keyboard.h"

#include "ui/icons/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

// Larguras relativas do canvas (flex) x5, porque o flex_grow do LVGL é inteiro.
constexpr uint8_t kKeyGrow = 5, kSideGrow = 8, kModeGrow = 8, kSpaceGrow = 25, kActionGrow = 12;

enum class Mode : uint8_t { Letters, Upper, Digits, Symbols };

const char *const kSearchLetters[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM-"};
// Os nomes só usam espaço ' - . : (♀/♂ viram " F"/" M" no build_sd.py) e a
// busca ignora pontuação; o 123 fica com dígitos e esses sinais.
const char *const kSearchDigits[3] = {"1234567890", "-.':", ""};

// Senha: só ASCII (o que a maioria dos roteadores aceita, e tudo tem glifo).
const char *const kLower[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
const char *const kUpper[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
const char *const kDigits[3] = {"1234567890", "-/:;()$&@\"", ".,?!'"};
const char *const kSymbols[3] = {"[]{}#%^*+=", "_\\|~<>`", ".,?!'"};

lv_obj_t *s_root;
KbLayout s_layout;
Mode s_mode;
StrId s_action;
bool s_action_on = true;
KbCallbacks s_cb;
lv_obj_t *s_action_btn;
lv_obj_t *s_action_label;

void build();

lv_obj_t *key(lv_obj_t *row, uint8_t grow, uint32_t bg) {
  lv_obj_t *k = ui_box(row);
  lv_obj_set_height(k, 40);
  lv_obj_set_flex_grow(k, grow);
  lv_obj_set_style_radius(k, 4, 0);
  lv_obj_set_style_bg_color(k, c(bg), 0);
  lv_obj_set_style_bg_color(k, c(theme::FAINT_2), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
  return k;
}

lv_obj_t *key_row(lv_coord_t pad_hor) {
  lv_obj_t *row = ui_box(s_root);
  lv_obj_set_size(row, LV_PCT(100), 40);
  lv_obj_set_style_pad_hor(row, pad_hor, 0);
  lv_obj_set_style_pad_column(row, 2, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  return row;
}

void on_key(lv_event_t *e) {
  char ch = (char)(uintptr_t)lv_event_get_user_data(e);
  if (s_cb.on_char) s_cb.on_char(ch);
  if (s_mode == Mode::Upper) {  // ⇧ vale pra uma letra
    s_mode = Mode::Letters;
    build();
  }
}

void on_space(lv_event_t *) {
  if (s_cb.on_space) s_cb.on_space();
}
void on_backspace(lv_event_t *) {
  if (s_cb.on_backspace) s_cb.on_backspace();
}
void on_action(lv_event_t *) {
  if (s_action_on && s_cb.on_action) s_cb.on_action();
}

// Botão de baixo à esquerda: letras <-> números.
void on_mode(lv_event_t *) {
  bool letters = s_mode == Mode::Letters || s_mode == Mode::Upper;
  s_mode = letters ? Mode::Digits : Mode::Letters;
  build();
}

// Tecla da esquerda da 3a linha (só no Text): ⇧ ou 123 <-> #+=.
void on_lead(lv_event_t *) {
  switch (s_mode) {
    case Mode::Letters: s_mode = Mode::Upper; break;
    case Mode::Upper: s_mode = Mode::Letters; break;
    case Mode::Digits: s_mode = Mode::Symbols; break;
    case Mode::Symbols: s_mode = Mode::Digits; break;
  }
  build();
}

void paint_action() {
  if (!s_action_btn) return;
  lv_label_set_text(s_action_label, tr(s_action));
  lv_obj_set_style_bg_color(s_action_btn, c(s_action_on ? theme::ACCENT : theme::LINE), 0);
  lv_obj_set_style_text_color(s_action_label, c(s_action_on ? theme::ON_ACCENT : theme::FAINT), 0);
  if (s_action_on) lv_obj_add_flag(s_action_btn, LV_OBJ_FLAG_CLICKABLE);
  else lv_obj_clear_flag(s_action_btn, LV_OBJ_FLAG_CLICKABLE);
}

const char *const *rows_for_mode() {
  if (s_layout == KbLayout::Search) return s_mode == Mode::Digits ? kSearchDigits : kSearchLetters;
  switch (s_mode) {
    case Mode::Upper: return kUpper;
    case Mode::Digits: return kDigits;
    case Mode::Symbols: return kSymbols;
    default: return kLower;
  }
}

void build() {
  lv_obj_clean(s_root);
  bool letters = s_mode == Mode::Letters || s_mode == Mode::Upper;
  const char *const *rows = rows_for_mode();
  for (uint8_t r = 0; r < 3; r++) {
    // 2a linha do QWERTY recuada 15 px dos dois lados (canvas).
    lv_obj_t *row = key_row((r == 1 && letters) ? 15 : 0);
    if (r == 2 && s_layout == KbLayout::Text) {
      bool shift_on = s_mode == Mode::Upper;
      lv_obj_t *lead = key(row, kSideGrow, shift_on ? theme::TEXT : theme::LINE);
      if (letters) {
        lv_obj_center(ui_icon(lead, &icon_shift_18, shift_on ? theme::ON_ACCENT : theme::TEXT));
      } else {
        lv_obj_center(ui_label(lead, &font_12_700, theme::TEXT,
                               tr(s_mode == Mode::Digits ? STR_KB_MODE_SYMBOLS : STR_SEARCH_MODE_NUM)));
      }
      ui_on_click(lead, on_lead);
    }
    for (const char *p = rows[r]; *p; p++) {
      lv_obj_t *k = key(row, kKeyGrow, theme::SURFACE_2);
      char text[2] = {*p, '\0'};
      lv_obj_center(ui_label(k, &font_14_600, theme::TEXT, text));
      ui_on_click(k, on_key, (void *)(uintptr_t)(unsigned char)*p);
    }
    if (r == 2) {
      if (rows[r][0] == '\0') {
        // Linha só com o apagar: espaçador mantém a tecla do tamanho de sempre.
        lv_obj_t *sp = ui_box(row);
        lv_obj_set_height(sp, 1);
        lv_obj_set_flex_grow(sp, 8 * kKeyGrow);
      }
      lv_obj_t *bs = key(row, kSideGrow, theme::LINE);
      lv_obj_center(ui_icon(bs, &icon_backspace_18, theme::TEXT));
      ui_on_click(bs, on_backspace);
    }
  }

  lv_obj_t *row = key_row(0);
  lv_obj_t *mode = key(row, kModeGrow, theme::LINE);
  StrId mode_label = !letters ? (s_layout == KbLayout::Search ? STR_SEARCH_MODE_ABC : STR_KB_MODE_LOWER)
                              : STR_SEARCH_MODE_NUM;
  lv_obj_center(ui_label(mode, &font_12_700, theme::TEXT, tr(mode_label)));
  ui_on_click(mode, on_mode);
  lv_obj_t *space = key(row, kSpaceGrow, theme::SURFACE_2);
  lv_obj_center(ui_label(space, &font_12_600, theme::MUTED, tr(STR_SEARCH_SPACE)));
  ui_on_click(space, on_space);
  s_action_btn = key(row, kActionGrow, theme::ACCENT);
  lv_obj_set_style_bg_color(s_action_btn, c(theme::TEXT), LV_STATE_PRESSED);
  s_action_label = ui_label(s_action_btn, &font_14_700, theme::ON_ACCENT, "");
  lv_obj_center(s_action_label);
  ui_on_click(s_action_btn, on_action);
  paint_action();
}

void on_delete(lv_event_t *) {
  s_root = nullptr;
  s_action_btn = nullptr;
  s_action_label = nullptr;
}

}  // namespace

void keyboard_create(lv_obj_t *parent, KbLayout layout, StrId action, const KbCallbacks &cb) {
  s_root = parent;
  s_layout = layout;
  s_mode = Mode::Letters;
  s_action = action;
  s_action_on = true;
  s_cb = cb;
  lv_obj_set_style_bg_color(parent, c(theme::SURFACE_DIM), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(parent, 2, 0);
  lv_obj_set_style_pad_row(parent, 2, 0);
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_event_cb(parent, on_delete, LV_EVENT_DELETE, nullptr);
  build();
}

void keyboard_set_action(StrId action, bool enabled) {
  s_action = action;
  s_action_on = enabled;
  paint_action();
}
