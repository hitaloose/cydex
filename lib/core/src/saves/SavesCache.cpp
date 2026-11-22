#include "SavesCache.h"

#include <cstring>

#include "utils/ByteUtils.h"

void saves_cache_header_encode(const SaveSignature &sig, uint8_t *out) {
  memset(out, 0, SAVES_CACHE_HEADER_SIZE);
  memcpy(out, "CYSV", 4);
  out[4] = (uint8_t)SAVES_CACHE_VERSION;
  out[5] = (uint8_t)(SAVES_CACHE_VERSION >> 8);
  out[6] = (uint8_t)sig.count;
  out[7] = (uint8_t)(sig.count >> 8);
  for (int i = 0; i < 4; i++) out[8 + i] = (uint8_t)(sig.value >> (8 * i));
}

bool saves_cache_header_decode(const uint8_t *d, const SaveSignature &expected, uint16_t *count) {
  if (memcmp(d, "CYSV", 4) != 0) return false;
  if (read_u16_le(d + 4) != SAVES_CACHE_VERSION) return false;
  if (read_u16_le(d + 6) != expected.count) return false;
  if (read_u32_le(d + 8) != expected.value) return false;
  *count = read_u16_le(d + 6);
  return true;
}

void saves_cache_record_encode(const SaveEntry &e, uint8_t *out) {
  memset(out, 0, SAVES_CACHE_RECORD_SIZE);
  strncpy((char *)out, e.file, SAVES_CACHE_NAME_SIZE - 1);
  out[128] = e.ok ? 1 : 0;
  if (e.ok) save_summary_encode(e.summary, out + SAVES_CACHE_SUMMARY_AT);
}

bool saves_cache_record_decode(const uint8_t *d, SaveEntry *out) {
  memset(out, 0, sizeof(*out));
  memcpy(out->file, d, SAVES_CACHE_NAME_SIZE);
  out->file[SAVES_CACHE_NAME_SIZE - 1] = 0;
  out->ok = d[128] == 1;
  if (out->ok && !save_summary_decode(d + SAVES_CACHE_SUMMARY_AT, &out->summary)) out->ok = false;
  return true;
}
