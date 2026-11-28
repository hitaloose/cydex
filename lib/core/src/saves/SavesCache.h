#pragma once

#include <cstddef>
#include <cstdint>

#include "dex/StatusCache.h"
#include "saves/SaveSummary.h"

// /cydex/saves.cache: um registro fixo por arquivo de /saves, gravado junto
// do status.cache (mesma assinatura). As telas de Saves, Hall da Fama, a
// Ficha ("Onde está") e a Vitrine ("Meu time") leem daqui com seek.
//
// Cabeçalho (16 B):
//   0  char[4] "CYSV"
//   4  u16     versão (SAVES_CACHE_VERSION)
//   6  u16     quantidade de registros
//   8  u32     assinatura dos saves (SaveSignature::value)
//   12 4 B     reservado
// Registro i em SAVES_CACHE_HEADER_SIZE + i * SAVES_CACHE_RECORD_SIZE:
//   0   char[128] nome do arquivo (sem a pasta)
//   128 u8        1 = reconhecido (o resumo vale), 0 = formato desconhecido
//   129 3 B       reservado
//   132 resumo (SAVE_SUMMARY_SIZE, saves/SaveSummary.h)

constexpr uint16_t SAVES_CACHE_VERSION = 2;  // 2: Gen4 (16 insígnias, onde está até 493)
constexpr size_t SAVES_CACHE_HEADER_SIZE = 16;
constexpr size_t SAVES_CACHE_NAME_SIZE = 128;
constexpr size_t SAVES_CACHE_SUMMARY_AT = 132;
constexpr size_t SAVES_CACHE_RECORD_SIZE = SAVES_CACHE_SUMMARY_AT + SAVE_SUMMARY_SIZE;

inline size_t saves_cache_record_offset(uint8_t i) {
  return SAVES_CACHE_HEADER_SIZE + (size_t)i * SAVES_CACHE_RECORD_SIZE;
}

struct SaveEntry {
  char file[SAVES_CACHE_NAME_SIZE];
  bool ok;
  SaveSummary summary;
};

void saves_cache_header_encode(const SaveSignature &sig, uint8_t *out);
// true se o cabeçalho for válido e a assinatura bater. `count` = registros.
bool saves_cache_header_decode(const uint8_t *data, const SaveSignature &expected, uint16_t *count);

void saves_cache_record_encode(const SaveEntry &e, uint8_t *out);
bool saves_cache_record_decode(const uint8_t *data, SaveEntry *out);
