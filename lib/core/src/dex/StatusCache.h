#pragma once

#include <cstddef>
#include <cstdint>

#include "DexStatus.h"

// /cydex/status.cache: evita reparsear os saves a cada boot.
//
//   0  char[4] "CYST"
//   4  u16     versão (STATUS_CACHE_VERSION)
//   6  u16     quantidade de saves
//   8  u32     assinatura dos saves (status_cache_signature)
//   12 u8[129] visto
//   141 u8[129] capturado
// Total STATUS_CACHE_SIZE bytes.
//
// Subir STATUS_CACHE_VERSION quando um parser novo (outra geração) entrar,
// pra invalidar caches antigos que ignoravam aquele formato.

constexpr uint16_t STATUS_CACHE_VERSION = 1;
constexpr size_t STATUS_CACHE_SIZE = 12 + 2 * DEX_BITS_BYTES;

// Acumula a assinatura arquivo a arquivo. Não depende da ordem em que o SD
// lista os arquivos (soma de hashes por arquivo).
struct SaveSignature {
  uint32_t value = 0;
  uint16_t count = 0;
  void add(const char *name, uint32_t size, uint32_t mtime);
};

void status_cache_encode(const SaveSignature &sig, const DexStatus &st, uint8_t *out);
// true só se o cache for válido e a assinatura bater com `expected`.
bool status_cache_decode(const uint8_t *data, size_t len, const SaveSignature &expected,
                         DexStatus *out);
