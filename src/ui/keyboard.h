#pragma once

#include <lvgl.h>

#include "i18n/i18n.h"

// Teclado de 40 px do canvas (Busca 08, Senha 05e). Um por vez na tela (o
// estado fica em estáticos daqui). Ocupa o `parent` inteiro (fundo
// surface-dim, padding 2, gap 2): 3 linhas de teclas + linha de baixo com
// modo, espaço e o botão principal.
//
//   Search: QWERTY maiúsculo / 123 (dígitos e - . ' :). Manda maiúsculas.
//   Text:   abc / ABC (⇧ volta pro abc depois de uma letra) / 123 / #+=.
enum class KbLayout : uint8_t { Search, Text };

struct KbCallbacks {
  void (*on_char)(char ch);
  void (*on_space)();
  void (*on_backspace)();
  void (*on_action)();
};

void keyboard_create(lv_obj_t *parent, KbLayout layout, StrId action, const KbCallbacks &cb);
// Troca o texto do botão principal e liga/desliga (apagado = sem toque).
void keyboard_set_action(StrId action, bool enabled);
