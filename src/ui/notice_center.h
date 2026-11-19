#pragma once

#include <lvgl.h>

#include "notify/Notice.h"

// Mostra os avisos do Claude Code que o hook_server enfileirou (15, 15b).
//
//  - Fora da Vitrine: faixa de 56 px em lv_layer_top(), por cima de qualquer
//    tela; trocar de tela não fecha. Toque fecha e mostra o próximo da fila.
//  - Na Vitrine: balão (showcase_show_notice). Toque no balão fecha só ele.
//  - Aviso novo acende a tela e toca dois bipes (se "Som ao tocar").
// Chamar no loop.
void notice_tick();

// Cor e textos de cada tipo (faixa e balão).
uint32_t notice_color(NoticeKind k);
const lv_img_dsc_t *notice_icon(NoticeKind k, bool small);
