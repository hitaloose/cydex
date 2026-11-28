#pragma once

#include <cstdint>

#include "saves/SaveMon.h"

// Criatura do Gen4 (PK4), layout do PKHeX (PK4.cs, PokeCrypto.cs):
//   0x00 PID u32 · 0x04 flags u16 (bit2 ovo ruim) · 0x06 checksum u16 (soma
//   u16 dos 128 B decifrados) · 0x08 128 B cifrados: 4 blocos de 32 B (A, B,
//   C, D) embaralhados por ((PID >> 13) & 31) % 24 e cifrados pelo LCRNG
//   (0x41C64E6D, 0x6073) com a semente no checksum.
//   A: 0x08 espécie (nº nacional) · 0x0A item · 0x0C TID · 0x0E SID
//      0x10 exp · 0x18 6 EVs
//   B: 0x28 4 golpes · 0x30 4 PP · 0x34 4 PP ups · 0x38 IV32 (bit30 ovo)
//   C: 0x48 apelido (11 × u16) · D: 0x68 nome do treinador (8 × u16)
// O de 236 B (time) acrescenta 100 B cifrados com a semente no PID:
//   0x8C nível · 0x8E PS atual · 0x90 PS máx, Atq, Def, Vel, AtE, DfE.

constexpr int GEN4_PK4_STORED_SIZE = 136;
constexpr int GEN4_PK4_PARTY_SIZE = 236;

// false se a vaga estiver vazia, o checksum não bater, for ovo ruim ou a
// espécie não existir no Gen4.
bool gen4_pk4_decode(const uint8_t *raw, bool party, SaveMon *out);

// Caminho inverso (testes): recebe o PK4 em claro, recalcula o checksum,
// embaralha e cifra no lugar (e a extensão do time, se `party`).
void gen4_pk4_encrypt(uint8_t *raw, bool party);
