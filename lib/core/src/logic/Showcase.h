#pragma once

#include <cstdint>

// Lógica da Vitrine (14) e das opções em degraus dos Ajustes, sem LVGL.

// Opções em degraus ("−" valor "+"). Retorna o índice de `value` em `steps`
// (ou o mais próximo abaixo; 0 se for menor que todos).
uint8_t option_index(const uint16_t *steps, uint8_t count, uint16_t value);
// Próximo valor andando `dir` (-1/+1), parando nas pontas.
uint16_t option_step(const uint16_t *steps, uint8_t count, uint16_t value, int8_t dir);

// Troca a cada: 30 s, 1, 2, 5, 15 min (CLAUDE.md, "Modo vitrine").
constexpr uint8_t SHOWCASE_INTERVAL_COUNT = 5;
extern const uint16_t SHOWCASE_INTERVALS_S[SHOWCASE_INTERVAL_COUNT];
// Apagar tela: 1, 2, 5, 10 min ou nunca (0, no fim da lista).
constexpr uint8_t SCREEN_OFF_COUNT = 5;
extern const uint16_t SCREEN_OFF_MIN[SCREEN_OFF_COUNT];

// Próxima posição na lista de espécies da vitrine (`count` itens).
// Sequencial: current+1, dando a volta. Aleatória: usa `rnd` e nunca repete a
// atual quando há mais de uma. `current` = -1 na primeira escolha.
uint16_t showcase_next(int32_t current, uint16_t count, bool random, uint32_t rnd);

// Deslocamento anti-retenção (1–2 px) da troca `k` (canvas: 4 posições).
void showcase_shift(uint32_t k, int8_t *dx, int8_t *dy);

// Minutos pra mostrar todas (resumo do 05b).
uint32_t showcase_cycle_minutes(uint16_t count, uint16_t interval_s);

// Colunas opacas (primeira e última) de um sprite quadrado RGB565 com fundo
// chroma key `key`. Falso se não houver pixel opaco.
bool showcase_opaque_cols(const uint16_t *px, uint16_t size, uint16_t key, uint8_t *x0,
                          uint8_t *x1);

// Zoom do LVGL (256 = 1x) pra uma parte opaca de `opaque_w` px caber em
// `avail` px: 2x (512) quando cabe, senão o maior que cabe.
uint16_t showcase_fit_zoom(uint16_t opaque_w, uint16_t avail);
