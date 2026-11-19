#pragma once

#include <cstdint>

#include "Dex.h"

constexpr int DEX_BITS_BYTES = (DEX_COUNT + 7) / 8;  // 129

enum class SpeciesState : uint8_t { Unknown, Seen, Caught };

// Visto/capturado das 1025 espécies, 1 bit cada (bit = n-1). Vive em RAM a
// execução inteira e é preenchido uma vez no boot a partir dos saves.
struct DexStatus {
  uint8_t seen[DEX_BITS_BYTES];
  uint8_t caught[DEX_BITS_BYTES];

  void clear();
  // Capturado implica visto.
  void mark_seen(uint16_t n);
  void mark_caught(uint16_t n);
  bool is_seen(uint16_t n) const;
  bool is_caught(uint16_t n) const;
  SpeciesState state(uint16_t n) const;
  uint16_t count_seen() const;
  uint16_t count_caught() const;
  // Capturados dentro de [first, last].
  uint16_t count_caught_range(uint16_t first, uint16_t last) const;
  // OR de outro status (vários saves).
  void merge(const DexStatus &other);
};
