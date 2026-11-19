#pragma once

#include <lvgl.h>

#include "i18n/i18n.h"

// Peças das telas de Ajustes (05, 05b, 05c), desenhadas a partir do canvas.
// Sem tema do LVGL: tudo estilizado aqui com os tokens.

// Linha de ajuste: x 0, largura 320, padding 0 10, gap 10, divisória embaixo.
lv_obj_t *set_row(lv_obj_t *scr, lv_coord_t y, lv_coord_t h, bool divider = true);
// Rótulo 14/600 + espaçador (os controles vão à direita).
lv_obj_t *set_label(lv_obj_t *row, StrId text);

// Slider 130 px (trilho 6 px, alvo de toque 24+). `on_change` a cada passo do
// dedo, `on_release` ao soltar (gravar na NVS aqui). Valor em lv_slider_get_value.
lv_obj_t *set_slider(lv_obj_t *row, int32_t min, int32_t max, int32_t value,
                     lv_event_cb_t on_change, lv_event_cb_t on_release);

// Interruptor 44x24. `on_toggle` recebe o novo estado via set_switch_get().
lv_obj_t *set_switch(lv_obj_t *row, bool on, lv_event_cb_t on_toggle);
bool set_switch_get(lv_obj_t *sw);

// Botões segmentados (Lista|Grade). `on_pick` recebe o índice em user_data
// do evento; chamar set_segmented_select pra pintar.
lv_obj_t *set_segmented(lv_obj_t *row, const StrId *labels, uint8_t count, uint8_t selected,
                        lv_event_cb_t on_pick, lv_coord_t pad_hor = 10);
void set_segmented_select(lv_obj_t *seg, uint8_t selected);

// "−" valor "+". `on_step` recebe -1/+1 em user_data. Atualizar com set_stepper_update.
struct Stepper {
  lv_obj_t *minus, *value, *plus;
};
Stepper set_stepper(lv_obj_t *row, lv_coord_t value_w, lv_event_cb_t on_step);
void set_stepper_update(const Stepper &st, const char *text, bool at_min, bool at_max);

// Linha "›" que abre outra tela: ícone accent, rótulo, resumo muted, seta.
lv_obj_t *set_link_row(lv_obj_t *scr, lv_coord_t y, lv_coord_t h, const lv_img_dsc_t *icon,
                       StrId label, const char *summary, lv_event_cb_t on_click, bool divider);

// "CYDex v0.1" à direita do cabeçalho.
void set_header_version(lv_obj_t *header);
