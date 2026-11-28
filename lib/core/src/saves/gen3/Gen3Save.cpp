#include "Gen3Save.h"

#include <cstring>

#include "Gen3Pk3.h"
#include "Gen3SaveParser.h"
#include "Gen3Sections.h"
#include "Gen3Species.h"
#include "Gen3TextDecoder.h"
#include "utils/ByteUtils.h"

namespace {

struct State {
  int8_t phys[GEN3_SECTIONS_PER_SLOT];
  Gen3GameVersion version;
  uint32_t key;
};
static_assert(sizeof(State) <= sizeof(SaveHandle::state), "estado do Gen3 não cabe");

const State &st(const SaveHandle &h) { return *(const State *)h.state; }

// Rascunho (SaveReader::scratch, 4 KB) pra tudo: Small block, setores do Hall
// da Fama, uma caixa. Nenhuma função abaixo usa dois ao mesmo tempo.
static_assert(GEN3_SECTOR_SIZE <= SAVE_SCRATCH_SIZE, "setor não cabe no rascunho");

bool read_section(int idx, uint8_t *out, void *user) {
  const SaveReader *r = (const SaveReader *)user;
  return r->read((size_t)idx * GEN3_SECTOR_SIZE, out, GEN3_SECTOR_SIZE, r->user);
}

// `len` bytes a partir do offset lógico `off` do bloco que começa na seção
// `first_id` (Large = 1, Storage = 5), atravessando as seções.
bool read_block(const SaveHandle &h, int first_id, size_t off, uint8_t *out, size_t len) {
  const State &s = st(h);
  while (len > 0) {
    int id = first_id + (int)(off / GEN3_SECTOR_DATA_SIZE);
    if (id >= GEN3_SECTIONS_PER_SLOT) return false;
    size_t inner = off % GEN3_SECTOR_DATA_SIZE;
    size_t chunk = GEN3_SECTOR_DATA_SIZE - inner;
    if (chunk > len) chunk = len;
    size_t at = (size_t)s.phys[id] * GEN3_SECTOR_SIZE + inner;
    if (!h.reader.read(at, out, chunk, h.reader.user)) return false;
    off += chunk;
    out += chunk;
    len -= chunk;
  }
  return true;
}

const Gen3LargeLayout &layout(Gen3GameVersion v) {
  switch (v) {
    case Gen3GameVersion::Emerald: return GEN3_LARGE_EMERALD;
    case Gen3GameVersion::FireRedLeafGreen: return GEN3_LARGE_FRLG;
    default: return GEN3_LARGE_RS;
  }
}

const Gen3BadgeLocation &badge_location(Gen3GameVersion v) {
  switch (v) {
    case Gen3GameVersion::Emerald: return GEN3_BADGE_LOCATION_EMERALD;
    case Gen3GameVersion::FireRedLeafGreen: return GEN3_BADGE_LOCATION_FRLG;
    default: return GEN3_BADGE_LOCATION_RS;
  }
}

SaveGame game_of(Gen3GameVersion v) {
  switch (v) {
    case Gen3GameVersion::Emerald: return SaveGame::Emerald;
    case Gen3GameVersion::FireRedLeafGreen: return SaveGame::FireRedLeafGreen;
    case Gen3GameVersion::RubySapphire: return SaveGame::RubySapphire;
    default: return SaveGame::Unknown;
  }
}

// Offset lógico `mo` nos dados do Hall da Fama (setor 28 e depois o 29).
bool read_hof(const SaveHandle &h, size_t mo, uint8_t *out, size_t len) {
  while (len > 0) {
    size_t at, room;
    if (mo < (size_t)GEN3_SECTOR_DATA_SIZE) {
      at = GEN3_HOF_SECTOR_A + mo;
      room = GEN3_SECTOR_DATA_SIZE - mo;
    } else {
      at = GEN3_HOF_SECTOR_B + (mo - GEN3_SECTOR_DATA_SIZE);
      room = 2 * GEN3_SECTOR_DATA_SIZE - mo;
    }
    size_t chunk = len < room ? len : room;
    if (at + chunk > h.reader.size) return false;
    if (!h.reader.read(at, out, chunk, h.reader.user)) return false;
    mo += chunk;
    out += chunk;
    len -= chunk;
  }
  return true;
}

// Setor extra íntegro: vazio (tudo 0x00 ou 0xFF) ou checksum em 0xFF4.
bool extra_sector_ok(const SaveHandle &h, long at, bool *empty) {
  uint8_t *sec = h.reader.scratch;
  *empty = true;
  if ((size_t)at + GEN3_SECTOR_SIZE > h.reader.size) return false;
  if (!h.reader.read((size_t)at, sec, GEN3_SECTOR_SIZE, h.reader.user)) return false;
  for (int i = 0; i < GEN3_SECTOR_SIZE; i++) {
    if (sec[i] != 0x00 && sec[i] != 0xFF) {
      *empty = false;
      break;
    }
  }
  if (*empty) return true;
  return read_u16_le(sec + GEN3_EXTRA_CHECKSUM_OFFSET) == gen3_checksum32(sec, GEN3_SECTOR_DATA_SIZE);
}

void decode_hof_mon(const uint8_t *p, SaveMon *m) {
  memset(m, 0, sizeof(*m));
  uint16_t spec_level = read_u16_le(p + 8);
  m->species = gen3_species_to_national(spec_level & 0x1FF);
  if (m->species == 0) return;
  m->tid = read_u16_le(p);
  m->sid = read_u16_le(p + 2);
  m->pid = read_u32_le(p + 4);
  m->level = (uint8_t)(spec_level >> 9);
  m->nature = (uint8_t)(m->pid % 25);
  m->shiny = gen3_is_shiny(m->tid, m->sid, m->pid);
  gen3_text_decode(p + 10, 10, m->nick, sizeof(m->nick));
}

uint32_t fnv(uint32_t h, uint32_t v) {
  for (int i = 0; i < 4; i++) {
    h ^= (v >> (8 * i)) & 0xFF;
    h *= 16777619u;
  }
  return h;
}

bool bit(const uint8_t *small, int base, uint16_t n) {
  int b = n - 1;
  return (small[base + (b >> 3)] >> (b & 7)) & 1;
}

}  // namespace

bool gen3_open(SaveHandle &h) {
  State s{};
  if (!gen3_find_active_slot(read_section, (void *)&h.reader, s.phys)) return false;
  uint8_t *small = h.reader.scratch;
  if (!small) return false;
  size_t at = (size_t)s.phys[GEN3_SMALL_BLOCK_SECTION_ID] * GEN3_SECTOR_SIZE;
  if (!h.reader.read(at, small, GEN3_SMALL_BLOCK_SIZE, h.reader.user)) return false;
  s.version = gen3_detect_version(small);
  if (s.version == Gen3GameVersion::Emerald) s.key = read_u32_le(small + GEN3_SMALL_SECURITY_KEY_EMERALD);
  else if (s.version == Gen3GameVersion::FireRedLeafGreen) s.key = read_u32_le(small + GEN3_SMALL_SECURITY_KEY_FRLG);
  memcpy(h.state, &s, sizeof(s));
  return true;
}

uint8_t gen3_read_party(const SaveHandle &h, SaveMon out[6]) {
  const Gen3LargeLayout &L = layout(st(h).version);
  uint8_t count = 0;
  if (!read_block(h, GEN3_LARGE_BLOCK_FIRST_SECTION_ID, L.party_count, &count, 1)) return 0;
  if (count > 6) count = 6;
  uint8_t raw[GEN3_PK3_PARTY_SIZE];
  uint8_t n = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (!read_block(h, GEN3_LARGE_BLOCK_FIRST_SECTION_ID, L.party + i * GEN3_PK3_PARTY_SIZE, raw,
                    sizeof(raw)))
      break;
    if (gen3_pk3_decode(raw, true, &out[n])) n++;
  }
  for (uint8_t i = n; i < 6; i++) memset(&out[i], 0, sizeof(SaveMon));
  return n;
}

bool gen3_read_box(const SaveHandle &h, uint8_t box, SaveMon out[SAVE_BOX_SLOTS], char *name,
                   size_t name_size) {
  if (box >= GEN3_BOX_COUNT) return false;
  constexpr size_t kBox = GEN3_BOX_SLOTS * GEN3_PK3_STORED_SIZE;  // 2400 B
  uint8_t *raw = h.reader.scratch;
  size_t off = GEN3_STORAGE_BOXES_OFFSET + (size_t)box * kBox;
  if (!read_block(h, GEN3_STORAGE_FIRST_SECTION_ID, off, raw, kBox)) return false;
  for (int i = 0; i < GEN3_BOX_SLOTS; i++) {
    if (!gen3_pk3_decode(raw + i * GEN3_PK3_STORED_SIZE, false, &out[i])) memset(&out[i], 0, sizeof(SaveMon));
  }
  if (name && name_size) {
    uint8_t n[GEN3_BOX_NAME_LENGTH];
    if (read_block(h, GEN3_STORAGE_FIRST_SECTION_ID,
                   GEN3_STORAGE_BOX_NAMES_OFFSET + box * GEN3_BOX_NAME_LENGTH, n, sizeof(n)))
      gen3_text_decode(n, GEN3_BOX_NAME_LENGTH, name, name_size);
    else
      name[0] = 0;
  }
  return true;
}

uint8_t gen3_read_hof_team(const SaveHandle &h, uint8_t team, SaveMon out[6]) {
  if (team >= GEN3_HOF_TEAMS) return 0;
  uint8_t raw[GEN3_HOF_TEAM_SIZE];
  if (!read_hof(h, (size_t)team * GEN3_HOF_TEAM_SIZE, raw, sizeof(raw))) return 0;
  uint8_t n = 0;
  for (int i = 0; i < 6; i++) {
    decode_hof_mon(raw + i * GEN3_HOF_MON_SIZE, &out[n]);
    if (out[n].species) n++;
  }
  for (uint8_t i = n; i < 6; i++) memset(&out[i], 0, sizeof(SaveMon));
  return n;
}

bool gen3_summarize(const SaveHandle &h, SaveSummary &s) {
  memset(&s, 0, sizeof(s));
  const State &state = st(h);
  uint8_t *small = h.reader.scratch;  // lido e usado até o fim do bloco do Small
  size_t at = (size_t)state.phys[GEN3_SMALL_BLOCK_SECTION_ID] * GEN3_SECTOR_SIZE;
  if (!h.reader.read(at, small, GEN3_SMALL_BLOCK_SIZE, h.reader.user)) return false;

  s.game = game_of(state.version);
  gen3_text_decode(small + GEN3_SMALL_OT_NAME_OFFSET, GEN3_SMALL_OT_NAME_LENGTH, s.name, sizeof(s.name));
  s.gender = small[GEN3_SMALL_GENDER_OFFSET];
  s.tid = read_u16_le(small + GEN3_SMALL_TID_OFFSET);
  s.sid = read_u16_le(small + GEN3_SMALL_SID_OFFSET);
  s.hours = read_u16_le(small + GEN3_SMALL_PLAYED_HOURS_OFFSET);
  s.minutes = small[GEN3_SMALL_PLAYED_MINUTES_OFFSET];
  s.seconds = small[GEN3_SMALL_PLAYED_SECONDS_OFFSET];
  s.dex_total = GEN3_NATIONAL_DEX_SPECIES_COUNT;
  s.badge_total = 8;
  for (uint16_t n = 1; n <= GEN3_NATIONAL_DEX_SPECIES_COUNT; n++) {
    bool caught = bit(small, GEN3_SMALL_POKEDEX_CAUGHT_OFFSET, n);
    if (caught) s.caught++;
    if (caught || bit(small, GEN3_SMALL_POKEDEX_SEEN_OFFSET, n)) s.seen++;
  }

  const Gen3LargeLayout &L = layout(state.version);
  uint8_t b4[4];
  if (read_block(h, GEN3_LARGE_BLOCK_FIRST_SECTION_ID, L.money, b4, 4)) s.money = read_u32_le(b4) ^ state.key;
  const Gen3BadgeLocation &B = badge_location(state.version);
  for (int i = 0; i < 8; i++) {
    int flag = B.badge_flag_start + i;
    uint8_t byte = 0;
    if (read_block(h, GEN3_LARGE_BLOCK_FIRST_SECTION_ID, B.event_flag_base + (flag >> 3), &byte, 1) &&
        ((byte >> (flag & 7)) & 1))
      s.badges |= (uint16_t)(1 << i);
  }

  // Uma criatura por vez (a RAM estática não comporta uma caixa decodificada).
  SaveMon m;
  uint8_t party_n = 0;
  if (read_block(h, GEN3_LARGE_BLOCK_FIRST_SECTION_ID, L.party_count, &party_n, 1) && party_n > 6) party_n = 6;
  for (uint8_t i = 0; i < party_n; i++) {
    uint8_t raw[GEN3_PK3_PARTY_SIZE];
    if (!read_block(h, GEN3_LARGE_BLOCK_FIRST_SECTION_ID, L.party + i * GEN3_PK3_PARTY_SIZE, raw, sizeof(raw)))
      break;
    if (!gen3_pk3_decode(raw, true, &m)) continue;
    SavePartyBrief &p = s.party[s.party_count++];
    p.species = m.species;
    p.level = m.level;
    p.flags = (m.shiny ? SAVE_MON_SHINY : 0) | (m.egg ? SAVE_MON_EGG : 0);
    strncpy(p.nick, m.nick, sizeof(p.nick) - 1);
    if (m.shiny) s.shiny_count++;
    if (!m.egg && m.species <= SAVE_WHERE_SPECIES) s.where[m.species - 1] = SAVE_WHERE_PARTY;
  }

  s.box_total = GEN3_BOX_COUNT;
  constexpr size_t kBox = GEN3_BOX_SLOTS * GEN3_PK3_STORED_SIZE;
  for (uint8_t b = 0; b < GEN3_BOX_COUNT; b++) {
    if (!read_block(h, GEN3_STORAGE_FIRST_SECTION_ID, GEN3_STORAGE_BOXES_OFFSET + (size_t)b * kBox, h.reader.scratch,
                    kBox))
      break;
    for (int i = 0; i < GEN3_BOX_SLOTS; i++) {
      if (!gen3_pk3_decode(h.reader.scratch + i * GEN3_PK3_STORED_SIZE, false, &m)) continue;
      s.box_count++;
      if (m.shiny) s.shiny_count++;
      if (!m.egg && m.species <= SAVE_WHERE_SPECIES && s.where[m.species - 1] == SAVE_WHERE_NONE)
        s.where[m.species - 1] = (uint8_t)(b + 1);
    }
  }

  bool empty_a, empty_b;
  if (extra_sector_ok(h, GEN3_HOF_SECTOR_A, &empty_a) && extra_sector_ok(h, GEN3_HOF_SECTOR_B, &empty_b) &&
      !empty_a) {
    for (uint8_t t = 0; t < GEN3_HOF_TEAMS; t++) {
      uint8_t raw[GEN3_HOF_TEAM_SIZE];
      if (!read_hof(h, (size_t)t * GEN3_HOF_TEAM_SIZE, raw, sizeof(raw))) break;
      if ((read_u16_le(raw + 8) & 0x1FF) == 0) break;  // 1ª vaga livre: acabou
      uint32_t hash = 2166136261u;
      for (int i = 0; i < 6; i++) {
        hash = fnv(hash, read_u32_le(raw + i * GEN3_HOF_MON_SIZE));
        hash = fnv(hash, read_u32_le(raw + i * GEN3_HOF_MON_SIZE + 4));
      }
      s.hof_hash[s.hof_count++] = hash;
    }
  }
  if (s.hof_count > 0 &&
      read_block(h, GEN3_LARGE_BLOCK_FIRST_SECTION_ID, L.records + 4 * GEN3_RECORD_FIRST_HOF_PLAY_TIME, b4, 4))
    s.first_hof = read_u32_le(b4) ^ state.key;
  return true;
}
