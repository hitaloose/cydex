#include "Gen3SaveParser.h"

#include <cstring>

#include "utils/ByteUtils.h"

namespace {

struct SlotScanResult {
  bool all_sections_present;
  bool checksums_valid;
  uint32_t counter;  // contador da seção 0 (compara slot A vs B)
  // índice de seção física (0..27, ver Gen3SectionReader) de cada ID lógico
  // de seção (-1 = ausente).
  int section_physical_index[GEN3_SECTIONS_PER_SLOT];
};

// Seções não ficam em ordem física == ordem lógica no arquivo (o jogo roda
// entre elas pra desgastar a flash igualmente) — por isso é preciso ler o ID
// de cada seção física pra saber onde ela realmente pertence.
SlotScanResult scan_slot(Gen3SectionReader read_section, void *user_data, int slot_index) {
  SlotScanResult result{};
  result.checksums_valid = true;

  for (int i = 0; i < GEN3_SECTIONS_PER_SLOT; i++) {
    result.section_physical_index[i] = -1;
  }

  int slot_start_section = slot_index * GEN3_SECTIONS_PER_SLOT;
  bool seen[GEN3_SECTIONS_PER_SLOT] = {false};
  int present_count = 0;
  uint8_t section[GEN3_SECTOR_SIZE];

  for (int i = 0; i < GEN3_SECTIONS_PER_SLOT; i++) {
    int physical_index = slot_start_section + i;
    if (!read_section(physical_index, section, user_data)) {
      result.checksums_valid = false;  // falha de leitura também invalida o slot
      continue;
    }

    uint16_t id = read_u16_le(section + GEN3_SECTOR_FOOTER_ID_OFFSET);
    if (id >= GEN3_SECTIONS_PER_SLOT) {
      result.checksums_valid = false;  // ID fora da faixa também invalida o slot
      continue;
    }
    if (seen[id]) continue;  // ID duplicado (lixo/corrompido) — ignora a repetição
    seen[id] = true;
    present_count++;
    result.section_physical_index[id] = physical_index;

    uint16_t stored_checksum = read_u16_le(section + GEN3_SECTOR_FOOTER_CHECKSUM_OFFSET);
    uint16_t computed_checksum = gen3_checksum32(section, GEN3_SECTOR_DATA_SIZE);
    if (stored_checksum != computed_checksum) {
      result.checksums_valid = false;
    }

    if (id == GEN3_SMALL_BLOCK_SECTION_ID) {
      result.counter = read_u32_le(section + GEN3_SECTOR_FOOTER_COUNTER_OFFSET);
    }
  }

  result.all_sections_present = (present_count == GEN3_SECTIONS_PER_SLOT);
  return result;
}

Gen3GameVersion detect_version(const uint8_t *small_block) {
  uint32_t probe = read_u32_le(small_block + GEN3_SMALL_VERSION_PROBE_OFFSET);
  if (probe == 1) return Gen3GameVersion::FireRedLeafGreen;
  if (probe == 0) return Gen3GameVersion::RubySapphire;

  for (int i = GEN3_SMALL_EMERALD_ONLY_RANGE_START; i < GEN3_SMALL_EMERALD_ONLY_RANGE_END; i++) {
    if (small_block[i] != 0) return Gen3GameVersion::Emerald;
  }
  return Gen3GameVersion::RubySapphire;
}

struct MemoryReaderContext {
  const uint8_t *data;
  size_t size;
};

bool read_section_from_memory(int physical_section_index, uint8_t *out, void *user_data) {
  const MemoryReaderContext *ctx = (const MemoryReaderContext *)user_data;
  size_t offset = (size_t)physical_section_index * GEN3_SECTOR_SIZE;
  if (offset + GEN3_SECTOR_SIZE > ctx->size) return false;
  memcpy(out, ctx->data + offset, GEN3_SECTOR_SIZE);
  return true;
}

}  // namespace

namespace {

// Escolhe o slot ativo (A/B): maior contador entre os que têm as 14 seções
// com checksum válido. Retorna -1 se nenhum estiver íntegro.
int choose_slot(Gen3SectionReader read_section, void *user_data, SlotScanResult *out_slot) {
  SlotScanResult slots[2] = {scan_slot(read_section, user_data, 0),
                              scan_slot(read_section, user_data, 1)};
  bool ok0 = slots[0].all_sections_present && slots[0].checksums_valid;
  bool ok1 = slots[1].all_sections_present && slots[1].checksums_valid;

  int chosen;
  if (ok0 && ok1) {
    chosen = (slots[1].counter > slots[0].counter) ? 1 : 0;
  } else if (ok0) {
    chosen = 0;
  } else if (ok1) {
    chosen = 1;
  } else {
    return -1;
  }
  *out_slot = slots[chosen];
  return chosen;
}

}  // namespace

bool gen3_read_small_block(Gen3SectionReader read_section, void *user_data, uint8_t *out_small,
                           Gen3GameVersion *out_version) {
  SlotScanResult slot{};
  if (choose_slot(read_section, user_data, &slot) < 0) return false;

  uint8_t section[GEN3_SECTOR_SIZE];
  if (!read_section(slot.section_physical_index[GEN3_SMALL_BLOCK_SECTION_ID], section, user_data)) {
    return false;
  }
  memcpy(out_small, section, GEN3_SMALL_BLOCK_SIZE);
  if (out_version) *out_version = detect_version(out_small);
  return true;
}

Gen3ParsedSave gen3_parse_save_streaming(Gen3SectionReader read_section, void *user_data) {
  Gen3ParsedSave result{};
  result.valid = false;
  result.version = Gen3GameVersion::Unknown;

  SlotScanResult slot{};
  if (choose_slot(read_section, user_data, &slot) < 0) {
    return result;  // nenhum dos dois slots íntegro: marca inválido, não trava
  }

  uint8_t section[GEN3_SECTOR_SIZE];

  int small_idx = slot.section_physical_index[GEN3_SMALL_BLOCK_SECTION_ID];
  read_section(small_idx, section, user_data);
  memcpy(result.small_block, section, GEN3_SMALL_BLOCK_SIZE);

  for (int i = 0; i < GEN3_LARGE_BLOCK_SECTION_COUNT; i++) {
    int id = GEN3_LARGE_BLOCK_FIRST_SECTION_ID + i;
    int idx = slot.section_physical_index[id];
    read_section(idx, section, user_data);
    memcpy(result.large_block + (i * GEN3_SECTOR_DATA_SIZE), section, GEN3_SECTOR_DATA_SIZE);
  }

  result.version = detect_version(result.small_block);
  result.valid = true;
  return result;
}

Gen3ParsedSave gen3_parse_save(const uint8_t *save_data, size_t save_data_size) {
  if (save_data_size < (size_t)GEN3_SAVE_FILE_SIZE) {
    Gen3ParsedSave result{};
    result.valid = false;
    result.version = Gen3GameVersion::Unknown;
    return result;
  }

  MemoryReaderContext ctx{save_data, save_data_size};
  return gen3_parse_save_streaming(read_section_from_memory, &ctx);
}
