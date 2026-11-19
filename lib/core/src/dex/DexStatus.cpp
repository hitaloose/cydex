#include "DexStatus.h"

#include <cstring>

static inline bool valid(uint16_t n) { return n >= 1 && n <= DEX_COUNT; }
static inline bool get_bit(const uint8_t *bits, uint16_t n) {
  return (bits[(n - 1) >> 3] >> ((n - 1) & 7)) & 1;
}
static inline void set_bit(uint8_t *bits, uint16_t n) { bits[(n - 1) >> 3] |= 1 << ((n - 1) & 7); }

void DexStatus::clear() {
  memset(seen, 0, sizeof(seen));
  memset(caught, 0, sizeof(caught));
}

void DexStatus::mark_seen(uint16_t n) {
  if (valid(n)) set_bit(seen, n);
}

void DexStatus::mark_caught(uint16_t n) {
  if (!valid(n)) return;
  set_bit(caught, n);
  set_bit(seen, n);
}

bool DexStatus::is_seen(uint16_t n) const { return valid(n) && get_bit(seen, n); }
bool DexStatus::is_caught(uint16_t n) const { return valid(n) && get_bit(caught, n); }

SpeciesState DexStatus::state(uint16_t n) const {
  if (is_caught(n)) return SpeciesState::Caught;
  if (is_seen(n)) return SpeciesState::Seen;
  return SpeciesState::Unknown;
}

uint16_t DexStatus::count_seen() const {
  uint16_t c = 0;
  for (uint16_t n = 1; n <= DEX_COUNT; n++) c += is_seen(n);
  return c;
}

uint16_t DexStatus::count_caught() const { return count_caught_range(1, DEX_COUNT); }

uint16_t DexStatus::count_caught_range(uint16_t first, uint16_t last) const {
  uint16_t c = 0;
  for (uint16_t n = first; n <= last && n <= DEX_COUNT; n++) c += is_caught(n);
  return c;
}

void DexStatus::merge(const DexStatus &o) {
  for (int i = 0; i < DEX_BITS_BYTES; i++) {
    seen[i] |= o.seen[i];
    caught[i] |= o.caught[i];
  }
}
