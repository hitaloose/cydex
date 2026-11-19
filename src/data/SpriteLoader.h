#pragma once

#include <lvgl.h>

#include <cstdint>

// Sprites do SD: um arquivo por tamanho (/cydex/sprites_<px>.bin, formato em
// dex/SpriteFile.h). Cada registro é lv_img_header_t + RGB565 com fundo
// chroma key 0x00FF00 (LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED).
//
// Tamanhos, todos gerados no PC. No aparelho nunca há escala fracionária:
//   96 -> Ficha (1x) e Vitrine (zoom 2x, vizinho mais próximo)
//   64 -> Evolução (2/3 de 96, reduzido no PC por tools/build_sd.py)
//   48 -> Grade

enum class SpriteSize : uint8_t { S96, S64, S48 };

struct Sprite {
  lv_img_dsc_t dsc;
  uint8_t *buf;  // malloc; liberar com sprite_free
};

// `out` precisa começar zerado (estático ou {}); se já tiver buffer, ele é liberado.
bool sprite_load(uint16_t n, SpriteSize size, Sprite *out);
void sprite_free(Sprite *s);
