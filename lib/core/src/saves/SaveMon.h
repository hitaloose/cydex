#pragma once

#include <cstdint>

// Uma criatura de um save (time, caixa ou Hall da Fama), já no formato que a
// UI usa, igual pra qualquer geração. Cada parser preenche este struct.
//
// Status na ordem do species.bin: PS, Atq, Def, AtE, DfE, Vel.
// Quem vem da caixa não traz nível nem status: `level` = 0 e `stats` zerados
// até mon_fill_stats() (logic/MonStats), que precisa da base e da curva de
// crescimento da espécie (species.bin).
struct SaveMon {
  uint16_t species;  // nº nacional; 0 = vaga vazia
  char nick[24];     // UTF-8
  char ot[24];
  uint16_t tid, sid;
  uint32_t pid;
  uint32_t exp;
  uint8_t level;   // 0 = desconhecido (caixa antes de mon_fill_stats)
  uint8_t nature;  // 0..24
  bool shiny;
  bool egg;
  uint16_t item;  // índice do item no jogo (0 = nenhum)
  uint16_t moves[4];
  uint8_t pp[4];
  uint8_t pp_ups[4];  // 0..3
  uint8_t ivs[6];
  uint8_t evs[6];
  uint16_t hp;  // PS atual (só no time; na caixa = stats[0])
  uint16_t stats[6];
};

constexpr uint8_t SAVE_STAT_COUNT = 6;
