#pragma once

#include <TFT_eSPI.h>
#include <lvgl.h>

extern TFT_eSPI tft;

// Liga o painel em paisagem (320x240) e limpa a tela.
void display_init();

// Barras de cor + texto, sem LVGL: confere rotação e ordem de cores.
void display_self_test();

// Registra o driver de display no LVGL (buffer parcial em DRAM interna).
void display_lvgl_init();
