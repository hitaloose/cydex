#pragma once

#include <cstdint>

#include "saves/SaveMon.h"

// Contas de nível e status (fórmulas da Gen3 em diante), sem I/O.

// Curva de crescimento, com os ids da PokeAPI (growth-rate/1..6). 0 = não se
// sabe (species.bin antigo, sem o byte).
enum class GrowthRate : uint8_t {
  Unknown = 0,
  Slow = 1,
  Medium = 2,  // "medium fast"
  Fast = 3,
  MediumSlow = 4,
  Erratic = 5,      // "slow-then-very-fast"
  Fluctuating = 6,  // "fast-then-very-slow"
};

// Experiência total pra chegar no nível `level` (1..100).
uint32_t exp_for_level(GrowthRate rate, uint8_t level);
// Maior nível (1..100) cuja experiência é <= `exp`. 0 com curva desconhecida.
uint8_t level_from_exp(GrowthRate rate, uint32_t exp);

// Natureza 0..24: qual status sobe e qual desce (índices na ordem do
// species.bin: 1 Atq, 2 Def, 3 AtE, 4 DfE, 5 Vel). Neutra: os dois iguais.
void nature_effect(uint8_t nature, uint8_t *up, uint8_t *down);

// Status de um índice (0 = PS) a partir da base, IV, EV, nível e natureza.
uint16_t mon_stat(uint8_t index, uint8_t base, uint8_t iv, uint8_t ev, uint8_t level,
                  uint8_t nature);

// Completa nível e status de quem veio da caixa (level == 0). `base` na ordem
// do species.bin. Com curva desconhecida, deixa tudo zerado. Shedinja (292)
// tem sempre 1 de PS.
void mon_fill_stats(SaveMon &m, const uint8_t base[6], GrowthRate rate);
