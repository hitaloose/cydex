#pragma once

#include <cstdint>

#include "saves/SaveMon.h"

// Criatura do Gen3 (PK3), layout do PKHeX (PK3.cs, PokeCrypto.cs):
//   0x00 PID u32 · 0x04 OTID u32 (TID u16, SID u16) · 0x08 apelido (10 B)
//   0x12 idioma · 0x13 flags (bit0 ovo ruim, bit1 tem espécie, bit2 ovo)
//   0x14 nome do treinador (7 B) · 0x1C checksum u16 (soma u16 dos 48 B
//   decifrados) · 0x20 48 B cifrados: 4 subestruturas de 12 B (Growth,
//   Attacks, EVs, Misc), embaralhadas por PID % 24 e com cada u32 XOR
//   (PID ^ OTID).
// O de 100 B (time) acrescenta: 0x54 nível, 0x56 PS atual, 0x58 PS máx,
// 0x5A Atq, 0x5C Def, 0x5E Vel, 0x60 AtE, 0x62 DfE.

constexpr int GEN3_PK3_STORED_SIZE = 80;
constexpr int GEN3_PK3_PARTY_SIZE = 100;

// false se a vaga estiver vazia, o checksum não bater, for "ovo ruim" ou a
// espécie não existir. `party` = registro de 100 B (traz nível e status).
bool gen3_pk3_decode(const uint8_t *raw, bool party, SaveMon *out);

// Brilhante: (TID ^ SID ^ PID alto ^ PID baixo) < 8.
bool gen3_is_shiny(uint16_t tid, uint16_t sid, uint32_t pid);

// Caminho inverso (os testes montam PK3 decifrados e cifram com isto):
// recalcula o checksum dos 48 B em claro, embaralha e cifra no lugar.
void gen3_pk3_encrypt(uint8_t *raw);
