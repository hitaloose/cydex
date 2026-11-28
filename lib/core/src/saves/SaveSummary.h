#pragma once

#include <cstddef>
#include <cstdint>

// Resumo de um save, igual pra qualquer geração. Montado no boot (só quando
// a assinatura dos saves muda) e guardado no /cydex/saves.cache
// (saves/SavesCache.h). As telas leem daqui, sem reabrir o save.

enum class SaveGame : uint8_t {
  Unknown = 0,
  RubySapphire = 1,  // o Gen3 não separa Ruby de Sapphire
  Emerald = 2,
  FireRedLeafGreen = 3,  // nem FireRed de LeafGreen
  HeartGoldSoulSilver = 4,  // Gen4; nem HeartGold de SoulSilver
  // Reservados: 5 = Diamond/Pearl, 6 = Platinum.
};
constexpr SaveGame SAVE_GAME_LAST = SaveGame::HeartGoldSoulSilver;

constexpr uint8_t SAVE_PARTY_MAX = 6;
constexpr uint8_t SAVE_HOF_MAX = 50;
// "Onde está": 1 byte por espécie até a dex do Gen4.
constexpr uint16_t SAVE_WHERE_SPECIES = 493;
constexpr uint8_t SAVE_BADGES_MAX = 16;  // HGSS: Johto (bits 0..7) + Kanto (8..15)
constexpr uint8_t SAVE_WHERE_NONE = 0;
constexpr uint8_t SAVE_WHERE_PARTY = 0xFF;  // 1..14 = nº da caixa

constexpr uint8_t SAVE_MON_SHINY = 0x01;
constexpr uint8_t SAVE_MON_EGG = 0x02;

struct SavePartyBrief {
  uint16_t species;  // nacional; 0 = vaga vazia
  uint8_t level;
  uint8_t flags;  // SAVE_MON_*
  char nick[24];
};

struct SaveSummary {
  SaveGame game;
  char name[24];
  uint8_t gender;  // 0 masculino, 1 feminino
  uint16_t tid, sid;
  uint16_t hours;
  uint8_t minutes, seconds;
  uint32_t money;
  uint16_t badges;      // bit i = insígnia i
  uint8_t badge_total;  // 8 ou 16
  uint16_t seen, caught, dex_total;
  uint8_t hof_count;
  uint32_t first_hof;  // tempo da 1ª vitória: h << 16 | min << 8 | s (0 = nenhuma)
  uint16_t shiny_count;  // time + caixas
  uint16_t box_count;    // criaturas nas caixas
  uint8_t box_total;     // quantas caixas o jogo tem
  uint8_t party_count;
  SavePartyBrief party[SAVE_PARTY_MAX];
  // Um hash por time do Hall da Fama (PID + ID das 6 vagas): o mesmo time em
  // dois saves (cópia) conta uma vez.
  uint32_t hof_hash[SAVE_HOF_MAX];
  uint8_t where[SAVE_WHERE_SPECIES];  // SAVE_WHERE_*; índice = nº − 1

  uint8_t badge_count() const;
  // Lugar da espécie `n` neste save (SAVE_WHERE_*).
  uint8_t where_of(uint16_t n) const;
};

// Só os campos fixos do começo do resumo (os primeiros SAVE_HEAD_SIZE bytes):
// o que as listas mostram, sem os ~800 B de time, hashes e "onde está".
struct SaveHead {
  SaveGame game;
  char name[24];
  uint8_t gender;
  uint16_t tid;
  uint16_t hours;
  uint8_t minutes;
  uint16_t badges;
  uint8_t badge_total;
  uint16_t seen, caught, dex_total;
  uint8_t hof_count;
  uint32_t first_hof;
  uint16_t box_count;
  uint8_t box_total;
  uint8_t party_count;
};
constexpr size_t SAVE_HEAD_SIZE = 58;
constexpr size_t SAVE_SUMMARY_PARTY_AT = 64;
constexpr size_t SAVE_PARTY_BRIEF_SIZE = 28;

constexpr size_t SAVE_SUMMARY_SIZE = 928;
// Offsets no resumo codificado, pra ler só um pedaço com seek.
constexpr size_t SAVE_SUMMARY_HOF_COUNT_AT = 45;
constexpr size_t SAVE_SUMMARY_HOF_HASH_AT = 232;
constexpr size_t SAVE_SUMMARY_WHERE_AT = 432;

void save_summary_encode(const SaveSummary &s, uint8_t *out);
bool save_summary_decode(const uint8_t *data, SaveSummary *out);
bool save_head_decode(const uint8_t *data, SaveHead *out);
// Uma vaga do time (SAVE_PARTY_BRIEF_SIZE bytes). false se vazia.
bool save_party_brief_decode(const uint8_t *data, SavePartyBrief *out);
