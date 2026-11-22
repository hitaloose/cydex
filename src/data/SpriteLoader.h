#pragma once

#include <lvgl.h>

#include <cstdint>

// Sprites do SD: um arquivo por tamanho (/cydex/sprites_<px>.bin, formato em
// dex/SpriteFile.h). Cada registro é lv_img_header_t + RGB565 com fundo
// chroma key 0x00FF00 (LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED).
//
// Tamanhos, todos gerados no PC. Só a Vitrine usa escala fracionária:
//   96 -> Ficha (1x) e Vitrine (até 2x, vizinho mais próximo, reduzido pra
//         caber no palco de 164 px)
//   64 -> Evolução (2/3 de 96, reduzido no PC por tools/build_sd.py)
//   48 -> Grade, Time do save, Hall da Fama
//   24 -> Caixas do save e lista do Hall da Fama (metade do 48)

enum class SpriteSize : uint8_t { S96, S64, S48, S24 };

struct Sprite {
  lv_img_dsc_t dsc;
  uint8_t *buf;  // malloc; liberar com sprite_free
};

// Reserva a área fixa dos sprites (46 KB = 40 vagas de 1152 B), pedida no
// setup antes do Wi-Fi. Com o Wi-Fi conectado (~70 KB de heap) não cabiam 10
// sprites soltos na Grade (os 3 últimos falhavam) nem um 96 de 18 KB.
// Um 24 ocupa 1 vaga, um 48 ocupa 4, um 64 ocupa 8 e um 96 ocupa 16. O pior
// caso de cada tela cabe: Grade 10x48; Ficha 96 + 3x64; Vitrine 96; Caixas
// 30x24 + 48; Hall da Fama 18x24. As telas nunca
// coexistem (o nav apaga a anterior antes de montar a próxima). Sem vaga, cai
// no malloc. Chamar uma vez no setup.
void sprite_reserve();

// Empresta vagas da área dos sprites pra outra coisa da tela (as telas de
// Saves guardam ali time e caixa decodificados: sobra área, falta RAM
// estática). Sem vaga, cai no malloc. Devolver com sprite_arena_return.
void *sprite_arena_borrow(size_t bytes);
void sprite_arena_return(void *p);

// `out` precisa começar zerado (estático ou {}); se já tiver buffer, ele é liberado.
bool sprite_load(uint16_t n, SpriteSize size, Sprite *out);
void sprite_free(Sprite *s);
