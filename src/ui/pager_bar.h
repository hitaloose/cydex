#pragma once

#include <lvgl.h>

#include "logic/Pager.h"

// Rodapé de paginação (canvas: Lista/Grade): ◀ 72 px | faixa + "pág X de Y" | ▶ 72 px.
// Segurar ◀ ▶ repete (HoldRepeat). Tocar no meio chama on_center (abre Ir para).
//
// A cada mudança de página chama on_page(holding): `holding` = ainda com o
// dedo na seta. A Grade usa isso pra só carregar sprites quando soltar.
struct PagerBar {
  lv_obj_t *root;
  lv_obj_t *prev;
  lv_obj_t *next;
  lv_obj_t *range;
  lv_obj_t *page;
  Pager pager;
  HoldRepeat hold;
  int8_t hold_dir;
  void (*on_page)(bool holding);
  void (*on_center)();
};

// `bar` precisa viver tanto quanto a tela (estático no arquivo da tela).
void pager_bar_create(PagerBar *bar, lv_obj_t *screen);
// Atualiza setas (ativas/inativas) e os dois textos do meio.
void pager_bar_refresh(PagerBar *bar, const char *range_text);
