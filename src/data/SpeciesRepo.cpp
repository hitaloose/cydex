#include "SpeciesRepo.h"

#include <cstring>

#include "dex/Dex.h"
#include "hal/sd.h"
#include "paths.h"
#include "util/log.h"

static const char *TAG = "species";

static char s_names[DEX_COUNT][SPECIES_NAME_SIZE];
static uint8_t s_types[DEX_COUNT][2];
static uint16_t s_count = 0;

// Lê em blocos de 32 registros (2 KB): bem mais rápido que 1025 seeks.
static constexpr uint16_t kChunk = 32;

bool species_repo_init(SpeciesProgressCb cb, void *user) {
  s_count = 0;
  size_t size = 0;
  if (!sd_open_read(SPECIES_PATH, &size)) return false;

  uint8_t header[SPECIES_HEADER_SIZE];
  SpeciesFileHeader h{};
  if (!sd_read_at(0, header, sizeof(header)) || !species_header_decode(header, &h)) {
    LOG_E(TAG, "cabeçalho inválido em %s", SPECIES_PATH);
    sd_close_read();
    return false;
  }
  uint16_t count = h.count > DEX_COUNT ? DEX_COUNT : h.count;
  if (size < species_record_offset(count + 1)) {
    LOG_E(TAG, "arquivo truncado (%u bytes)", (unsigned)size);
    sd_close_read();
    return false;
  }

  static uint8_t buf[kChunk * SPECIES_RECORD_SIZE];
  for (uint16_t first = 1; first <= count; first += kChunk) {
    uint16_t n = (uint16_t)((count - first + 1) < kChunk ? (count - first + 1) : kChunk);
    if (!sd_read_at(species_record_offset(first), buf, n * SPECIES_RECORD_SIZE)) {
      LOG_E(TAG, "falha lendo registros a partir de #%u", first);
      sd_close_read();
      return false;
    }
    for (uint16_t i = 0; i < n; i++) {
      memcpy(s_names[first - 1 + i], buf + i * SPECIES_RECORD_SIZE, SPECIES_NAME_SIZE);
      s_names[first - 1 + i][SPECIES_NAME_SIZE - 1] = '\0';
      const uint8_t *rec = buf + i * SPECIES_RECORD_SIZE;
      s_types[first - 1 + i][0] = rec[16];  // offsets: ver dex/SpeciesRecord.h
      s_types[first - 1 + i][1] = rec[17];
    }
    if (cb) cb((uint16_t)(first - 1 + n), count, user);
  }
  sd_close_read();
  s_count = count;
  LOG_I(TAG, "%u espécies carregadas", s_count);
  return true;
}

uint16_t species_repo_count() { return s_count; }

const char *species_repo_name(uint16_t n) {
  if (n < 1 || n > s_count) return "";
  return s_names[n - 1];
}

void species_repo_types(uint16_t n, SpeciesType *t1, SpeciesType *t2) {
  if (n < 1 || n > s_count) {
    *t1 = *t2 = SpeciesType::None;
    return;
  }
  *t1 = s_types[n - 1][0] < SPECIES_TYPE_COUNT ? (SpeciesType)s_types[n - 1][0] : SpeciesType::None;
  *t2 = s_types[n - 1][1] < SPECIES_TYPE_COUNT ? (SpeciesType)s_types[n - 1][1] : SpeciesType::None;
}

bool species_repo_get(uint16_t n, SpeciesRecord *out) {
  if (n < 1 || n > s_count) return false;
  if (!sd_open_read(SPECIES_PATH, nullptr)) return false;
  uint8_t buf[SPECIES_RECORD_SIZE];
  bool ok = sd_read_at(species_record_offset(n), buf, sizeof(buf));  // arquivo fica aberto
  return ok && species_record_decode(buf, n, out);
}
