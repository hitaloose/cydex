#include "StatusCache.h"

#include <cstring>

#include "utils/ByteUtils.h"

static uint32_t fnv1a(const uint8_t *p, size_t n, uint32_t h) {
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

void SaveSignature::add(const char *name, uint32_t size, uint32_t mtime) {
  uint32_t h = fnv1a((const uint8_t *)name, strlen(name), 2166136261u);
  uint8_t tail[8] = {(uint8_t)size, (uint8_t)(size >> 8), (uint8_t)(size >> 16),
                     (uint8_t)(size >> 24), (uint8_t)mtime, (uint8_t)(mtime >> 8),
                     (uint8_t)(mtime >> 16), (uint8_t)(mtime >> 24)};
  h = fnv1a(tail, sizeof(tail), h);
  value += h;
  count++;
}

void status_cache_encode(const SaveSignature &sig, const DexStatus &st, uint8_t *out) {
  memcpy(out, "CYST", 4);
  out[4] = (uint8_t)STATUS_CACHE_VERSION;
  out[5] = (uint8_t)(STATUS_CACHE_VERSION >> 8);
  out[6] = (uint8_t)sig.count;
  out[7] = (uint8_t)(sig.count >> 8);
  for (int i = 0; i < 4; i++) out[8 + i] = (uint8_t)(sig.value >> (8 * i));
  memcpy(out + 12, st.seen, DEX_BITS_BYTES);
  memcpy(out + 12 + DEX_BITS_BYTES, st.caught, DEX_BITS_BYTES);
}

bool status_cache_decode(const uint8_t *d, size_t len, const SaveSignature &expected,
                         DexStatus *out) {
  if (len != STATUS_CACHE_SIZE) return false;
  if (memcmp(d, "CYST", 4) != 0) return false;
  if (read_u16_le(d + 4) != STATUS_CACHE_VERSION) return false;
  if (read_u16_le(d + 6) != expected.count) return false;
  if (read_u32_le(d + 8) != expected.value) return false;
  memcpy(out->seen, d + 12, DEX_BITS_BYTES);
  memcpy(out->caught, d + 12 + DEX_BITS_BYTES, DEX_BITS_BYTES);
  return true;
}
