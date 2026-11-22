#include "Gen3Dex.h"

#include "Gen3Save.h"
#include "Gen3SaveParser.h"
#include "utils/ByteUtils.h"

namespace {

// Dumps de emulador às vezes têm bytes extras no fim (RTC etc.). Só os
// primeiros 128 KB importam. 256 KB já seria outro formato (Gen4 tem 512 KB).
bool detect(const SaveReader &r) {
  return r.size >= (size_t)GEN3_SAVE_FILE_SIZE && r.size < (size_t)GEN3_SAVE_FILE_SIZE * 2;
}

bool read_section(int idx, uint8_t *out, void *user) {
  const SaveReader *r = (const SaveReader *)user;
  return r->read((size_t)idx * GEN3_SECTOR_SIZE, out, GEN3_SECTOR_SIZE, r->user);
}

bool get_bit(const uint8_t *small, int base, uint16_t n) {
  int bit = n - 1;
  return (small[base + (bit >> 3)] >> (bit & 7)) & 1;
}

bool merge_dex(const SaveReader &r, DexStatus &out) {
  // No rascunho de quem chama: fora da pilha (o PokEsp estourou a pilha
  // nessa cadeia) e sem um buffer estático só pra isto.
  static_assert(GEN3_SMALL_BLOCK_SIZE <= SAVE_SCRATCH_SIZE, "Small block não cabe no rascunho");
  if (!r.scratch) return false;
  uint8_t *small = r.scratch;
  if (!gen3_read_small_block(read_section, (void *)&r, small, nullptr)) return false;

  // O bitfield do Gen3 já usa a ordem da dex nacional (bit = n-1).
  for (uint16_t n = 1; n <= GEN3_NATIONAL_DEX_SPECIES_COUNT; n++) {
    if (get_bit(small, GEN3_SMALL_POKEDEX_CAUGHT_OFFSET, n)) out.mark_caught(n);
    else if (get_bit(small, GEN3_SMALL_POKEDEX_SEEN_OFFSET, n)) out.mark_seen(n);
  }
  return true;
}

}  // namespace

const SaveFormat GEN3_SAVE_FORMAT = {
    "gen3", detect, merge_dex, gen3_open, gen3_summarize, gen3_read_party, gen3_read_box, gen3_read_hof_team,
};
