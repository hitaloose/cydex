#pragma once

#include <cstddef>
#include <cstdint>

#include "saves/SaveSummary.h"

// Save do Gen4 (NDS, 512 KB): duas cópias de 256 KB (0x0 e 0x40000). Cada
// cópia tem o bloco geral (treinador, time, dex) e o de armazenamento
// (caixas), e cada bloco termina num rodapé de 0x10 bytes:
//   -0x10 u32 contador de gravações · -0x0C u32 tamanho do bloco
//   -0x08 u32 mágico 0x20060623     · -0x02 u16 CRC16-CCITT (0xFFFF) de
//   tudo antes do rodapé
// Os dois blocos escolhem a cópia cada um por conta própria (o jogo nem
// sempre regrava as caixas).
//
// O Hall da Fama fica num bloco extra, também com duas cópias, e rodapé
// próprio no fim dos dados: +0 mágico, +4 contador, +8 tamanho. O CRC desse
// rodapé não bate com o CRC16 dos dados, então ele não é validado; cada
// criatura passa pelo teste de espécie.
//
// Offsets do PKHeX (SAV4HGSS, Zukan4, HallOfFame4), conferidos num save real
// de HeartGold. D/P/Pt são outra entrada (rodapé de 0x14, outros offsets).

constexpr size_t GEN4_SAVE_SIZE = 0x80000;
constexpr size_t GEN4_COPY_OFFSET = 0x40000;
constexpr uint32_t GEN4_FOOTER_MAGIC = 0x20060623;
constexpr uint16_t GEN4_DEX_SPECIES = 493;
constexpr uint8_t GEN4_BOX_SLOTS = 30;

struct Gen4Layout {
  SaveGame game;
  uint32_t general_size;  // com o rodapé
  uint32_t storage_at;
  uint32_t storage_size;
  uint8_t footer_size;
  // Bloco geral
  uint32_t trainer_name;  // 8 × u16
  uint32_t tid, sid, money, gender;
  uint32_t badges, badges2;  // 2º byte (Kanto no HGSS) ou 0
  uint8_t badge_total;
  uint32_t played_hours, played_minutes, played_seconds;
  uint32_t party_count, party;
  uint32_t dex;  // mágico; capturados em +4, vistos em +0x44 (0x40 B cada)
  // Bloco de armazenamento
  uint8_t box_count;
  uint32_t box_stride;  // 30 × 136 + folga
  uint32_t box_names;
  uint32_t box_name_stride;
  // Hall da Fama (do começo da cópia)
  uint32_t hof_at;
  uint32_t hof_data;  // 30 times × 0x16C + 8 B (total gravado em +0x2AA8)
};

constexpr uint32_t GEN4_DEX_CAUGHT = 0x04;
constexpr uint32_t GEN4_DEX_SEEN = 0x44;
constexpr uint32_t GEN4_DEX_BYTES = 0x40;

constexpr uint8_t GEN4_HOF_TEAMS = 30;
constexpr size_t GEN4_HOF_MON_SIZE = 0x3C;
constexpr size_t GEN4_HOF_TEAM_SIZE = 6 * GEN4_HOF_MON_SIZE + 4;  // + data (ano−2000 u16, mês, dia)
constexpr size_t GEN4_HOF_TOTAL_AT = GEN4_HOF_TEAMS * GEN4_HOF_TEAM_SIZE;  // 0x2AA8

constexpr Gen4Layout GEN4_LAYOUT_HGSS = {
    SaveGame::HeartGoldSoulSilver,
    0xF628, 0xF700, 0x12310, 0x10,
    0x64, 0x74, 0x76, 0x78, 0x7C,
    0x7E, 0x83, 16,
    0x86, 0x88, 0x89,
    0x94, 0x98,
    0x12B8,
    18, 0x1000, 0x12008, 0x28,
    0x23000, 0x2AB0,
};
