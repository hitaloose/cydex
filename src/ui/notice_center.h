#pragma once

#include <lvgl.h>

#include "notify/Notice.h"

// Mostra os avisos que o notice_server enfileirou (15, 15b).
//
//  - Fora da Vitrine: faixa de 56 px em lv_layer_top(), por cima de qualquer
//    tela; trocar de tela não fecha. Toque fecha e mostra o próximo da fila.
//  - Na Vitrine: balões (showcase_show_notices). Toque num balão fecha só ele.
//  - Aviso novo acende a tela (wake) e toca o chime (sound, se "Som ao tocar").
//  - Tira os que passaram do timeout.
// Chamar no loop.
void notice_tick();

// Ícone do aviso: 20 px na faixa, 16 px (small) no balão.
const lv_img_dsc_t *notice_icon(NoticeIcon icon, bool small);
