#include "Gen3PokedexInfo.h"

#include <cstring>

namespace {

bool get_bit(const uint8_t *small_block, int base_offset, uint16_t national_dex_number) {
  int bit = national_dex_number - 1;
  int byte_offset = base_offset + (bit >> 3);
  return (small_block[byte_offset] >> (bit & 7)) & 1;
}

}  // namespace

bool gen3_pokedex_is_caught(const Gen3ParsedSave &save, uint16_t national_dex_number) {
  return get_bit(save.small_block, GEN3_SMALL_POKEDEX_CAUGHT_OFFSET, national_dex_number);
}

bool gen3_pokedex_is_seen(const Gen3ParsedSave &save, uint16_t national_dex_number) {
  return get_bit(save.small_block, GEN3_SMALL_POKEDEX_SEEN_OFFSET, national_dex_number);
}

uint16_t gen3_pokedex_count_caught(const Gen3ParsedSave &save) {
  uint16_t count = 0;
  for (uint16_t species = 1; species <= GEN3_NATIONAL_DEX_SPECIES_COUNT; species++) {
    if (gen3_pokedex_is_caught(save, species)) count++;
  }
  return count;
}

uint16_t gen3_pokedex_count_seen(const Gen3ParsedSave &save) {
  uint16_t count = 0;
  for (uint16_t species = 1; species <= GEN3_NATIONAL_DEX_SPECIES_COUNT; species++) {
    if (gen3_pokedex_is_seen(save, species)) count++;
  }
  return count;
}

void gen3_pokedex_copy_caught_bitmask(const Gen3ParsedSave &save, uint8_t *out) {
  memcpy(out, save.small_block + GEN3_SMALL_POKEDEX_CAUGHT_OFFSET, GEN3_POKEDEX_BITMASK_BYTES);
}
