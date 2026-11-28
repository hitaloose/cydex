#include "SaveStore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "SpeciesRepo.h"
#include "hal/sd.h"
#include "logic/MonStats.h"
#include "paths.h"
#include "util/log.h"

static const char *TAG = "saves";

namespace {

uint8_t s_count = 0xFF;  // 0xFF = ainda não leu o cabeçalho
uint8_t s_ok = 0;
SaveHandle s_handle;
bool s_open = false;
uint8_t s_open_i = 0xFF;
char s_path[SAVES_CACHE_NAME_SIZE + 16];
size_t s_size = 0;

bool cache_read(size_t off, uint8_t *out, size_t len) {
  size_t size = 0;
  if (!sd_open_read(SAVES_CACHE_PATH, &size)) return false;
  if (off + len > size) return false;
  return sd_read_at(off, out, len);
}

// Conta os registros (e os reconhecidos). O cache só muda no boot / ao
// recarregar, e aí o nav remonta tudo: basta ler uma vez por boot.
void load_counts() {
  if (s_count != 0xFF) return;
  s_count = 0;
  s_ok = 0;
  size_t size = 0;
  if (!sd_open_read(SAVES_CACHE_PATH, &size) || size < SAVES_CACHE_HEADER_SIZE) return;
  uint8_t h[SAVES_CACHE_HEADER_SIZE];
  if (!sd_read_at(0, h, sizeof(h)) || memcmp(h, "CYSV", 4) != 0) return;
  uint16_t n = (uint16_t)(h[6] | (h[7] << 8));
  if (size < saves_cache_record_offset((uint8_t)n)) return;
  s_count = (uint8_t)(n > 16 ? 16 : n);
  for (uint8_t i = 0; i < s_count; i++) {
    uint8_t ok = 0;
    if (cache_read(saves_cache_record_offset(i) + SAVES_CACHE_NAME_SIZE, &ok, 1) && ok == 1) s_ok++;
  }
}

bool sd_reader(size_t off, uint8_t *out, size_t len, void *) {
  size_t size = 0;
  if (!sd_open_read(s_path, &size)) return false;
  return sd_read_at(off, out, len);
}

bool ensure_open() {
  if (!s_open) return false;
  size_t size = 0;
  return sd_open_read(s_path, &size);
}

}  // namespace

uint8_t *save_scratch() {
  alignas(4) static uint8_t s_buf[SAVE_SCRATCH_SIZE];
  return s_buf;
}

void save_store_invalidate() {
  s_count = 0xFF;
  s_open = false;
  s_open_i = 0xFF;
}

uint8_t save_store_count() {
  load_counts();
  return s_count;
}

uint8_t save_store_ok_count() {
  load_counts();
  return s_ok;
}

bool save_store_entry(uint8_t i, SaveEntry *out) {
  if (i >= save_store_count()) return false;
  // Na pilha (~1 KB): só a tela do Save chama, fora da cadeia funda do boot.
  uint8_t rec[SAVES_CACHE_RECORD_SIZE];
  if (!cache_read(saves_cache_record_offset(i), rec, sizeof(rec))) return false;
  return saves_cache_record_decode(rec, out);
}

bool save_store_head(uint8_t i, SaveHead *out) {
  if (i >= save_store_count()) return false;
  uint8_t ok = 0, d[SAVE_HEAD_SIZE];
  size_t base = saves_cache_record_offset(i);
  return cache_read(base + SAVES_CACHE_NAME_SIZE, &ok, 1) && ok == 1 &&
         cache_read(base + SAVES_CACHE_SUMMARY_AT, d, sizeof(d)) && save_head_decode(d, out);
}

bool save_store_party_brief(uint8_t i, uint8_t slot, SavePartyBrief *out) {
  if (slot >= SAVE_PARTY_MAX) return false;
  uint8_t d[SAVE_PARTY_BRIEF_SIZE];
  size_t at = saves_cache_record_offset(i) + SAVES_CACHE_SUMMARY_AT + SAVE_SUMMARY_PARTY_AT +
              (size_t)slot * SAVE_PARTY_BRIEF_SIZE;
  return cache_read(at, d, sizeof(d)) && save_party_brief_decode(d, out);
}

uint16_t save_store_team_total() {
  uint16_t n = 0;
  SaveHead h;
  for (uint8_t i = 0; i < save_store_count(); i++) {
    if (save_store_head(i, &h)) n += h.party_count;
  }
  return n;
}

bool save_store_file(uint8_t i, char *out, size_t size) {
  if (i >= save_store_count() || size == 0) return false;
  char name[SAVES_CACHE_NAME_SIZE];
  if (!cache_read(saves_cache_record_offset(i), (uint8_t *)name, sizeof(name))) return false;
  name[sizeof(name) - 1] = 0;
  snprintf(out, size, "%s", name);
  return true;
}

bool save_store_open(uint8_t i) {
  if (s_open && s_open_i == i) return ensure_open();
  s_open = false;
  if (i >= save_store_count()) return false;
  // Só o nome e o "reconhecido" do registro (sem decodificar o resumo).
  char name[SAVES_CACHE_NAME_SIZE + 1];
  if (!cache_read(saves_cache_record_offset(i), (uint8_t *)name, sizeof(name)) || name[SAVES_CACHE_NAME_SIZE] != 1)
    return false;
  name[SAVES_CACHE_NAME_SIZE - 1] = 0;
  snprintf(s_path, sizeof(s_path), "%s/%s", SAVES_DIR, name);
  if (!sd_open_read(s_path, &s_size)) return false;
  SaveReader r{s_size, sd_reader, nullptr, save_scratch()};
  if (!save_open(r, &s_handle)) {
    LOG_E(TAG, "%s: não abriu", name);
    return false;
  }
  s_open = true;
  s_open_i = i;
  return true;
}

uint8_t save_store_open_index() { return s_open ? s_open_i : 0xFF; }

uint8_t save_store_party(SaveMon out[6]) {
  if (!ensure_open()) return 0;
  return s_handle.format->read_party(s_handle, out);
}

bool save_store_box(uint8_t box, SaveMon out[SAVE_BOX_SLOTS], char *name, size_t name_size) {
  if (!ensure_open()) return false;
  if (!s_handle.format->read_box(s_handle, box, out, name, name_size)) return false;
  // Nível e status pela espécie (species.bin). Abre outro arquivo: o save é
  // reaberto na próxima leitura.
  for (uint8_t i = 0; i < SAVE_BOX_SLOTS; i++) {
    SaveMon &m = out[i];
    if (!m.species || m.level) continue;
    SpeciesRecord rec;
    if (species_repo_get(m.species, &rec)) mon_fill_stats(m, rec.stats, (GrowthRate)rec.growth);
  }
  return true;
}

uint8_t save_store_hof_team(uint8_t team, SaveMon out[6]) {
  if (!ensure_open()) return 0;
  return s_handle.format->read_hof_team(s_handle, team, out);
}

uint8_t save_store_where(uint16_t n, SaveWhere *out, uint8_t max) {
  if (n < 1 || n > SAVE_WHERE_SPECIES) return 0;
  uint8_t found = 0;
  for (uint8_t i = 0; i < save_store_count() && found < max; i++) {
    uint8_t rec[SAVES_CACHE_SUMMARY_AT - SAVES_CACHE_NAME_SIZE];
    size_t base = saves_cache_record_offset(i);
    if (!cache_read(base + SAVES_CACHE_NAME_SIZE, rec, 1) || rec[0] != 1) continue;
    uint8_t place = 0;
    if (!cache_read(base + SAVES_CACHE_SUMMARY_AT + SAVE_SUMMARY_WHERE_AT + (n - 1), &place, 1)) continue;
    if (place == SAVE_WHERE_NONE) continue;
    out[found++] = {i, place};
  }
  return found;
}

// Hashes do Hall da Fama do save `i` (os `count` primeiros). false se o
// registro não for reconhecido.
bool read_hof_hashes(uint8_t i, uint32_t *out, uint8_t *count) {
  size_t base = saves_cache_record_offset(i);
  uint8_t ok = 0, raw[SAVE_HOF_MAX * 4];
  *count = 0;
  if (!cache_read(base + SAVES_CACHE_NAME_SIZE, &ok, 1) || ok != 1) return false;
  if (!cache_read(base + SAVES_CACHE_SUMMARY_AT + SAVE_SUMMARY_HOF_COUNT_AT, count, 1) || *count > SAVE_HOF_MAX)
    return false;
  if (!cache_read(base + SAVES_CACHE_SUMMARY_AT + SAVE_SUMMARY_HOF_HASH_AT, raw, (size_t)*count * 4)) return false;
  for (uint8_t t = 0; t < *count; t++) {
    const uint8_t *p = raw + 4 * t;
    out[t] = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
  }
  return true;
}

uint16_t save_store_hof_list(HofRef *out, uint16_t max, uint8_t only_save) {
  // Sem tabela de "já vistos" na RAM (o heap com Wi-Fi não tem bloco pra
  // ela): cada time é comparado com os times dos saves anteriores, relidos do
  // saves.cache (200 B por save, na pilha).
  uint16_t n = 0;
  uint32_t mine[SAVE_HOF_MAX], other[SAVE_HOF_MAX];
  for (uint8_t i = 0; i < save_store_count(); i++) {
    uint8_t count = 0;
    if (!read_hof_hashes(i, mine, &count) || count == 0) continue;
    bool dup[SAVE_HOF_MAX] = {false};
    for (uint8_t j = 0; j < i; j++) {
      uint8_t oc = 0;
      if (!read_hof_hashes(j, other, &oc)) continue;
      for (uint8_t t = 0; t < count; t++) {
        for (uint8_t k = 0; k < oc && !dup[t]; k++) dup[t] = mine[t] == other[k];
      }
    }
    for (int t = count - 1; t >= 0; t--) {
      if (dup[t]) continue;
      if (only_save != 0xFF && only_save != i) continue;
      if (out && n < max) out[n] = {i, (uint8_t)t};
      n++;
    }
  }
  return n;
}

uint16_t save_store_hof_total() { return save_store_hof_list(nullptr, 0, 0xFF); }

const char *save_game_name(SaveGame g) {
  switch (g) {
    case SaveGame::RubySapphire: return "R/S";
    case SaveGame::Emerald: return "Emerald";
    case SaveGame::FireRedLeafGreen: return "FR/LG";
    case SaveGame::HeartGoldSoulSilver: return "HG/SS";
    default: return "?";
  }
}

uint32_t save_game_color(SaveGame g) {
  switch (g) {
    case SaveGame::RubySapphire: return 0xE8566B;
    case SaveGame::Emerald: return 0x6FCF6A;
    case SaveGame::FireRedLeafGreen: return 0xFF8A4C;
    case SaveGame::HeartGoldSoulSilver: return 0xE0B970;
    default: return 0x93A0AD;
  }
}
