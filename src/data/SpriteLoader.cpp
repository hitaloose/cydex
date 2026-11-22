#include "SpriteLoader.h"

#include <cstdlib>
#include <cstring>

#include "dex/SpriteFile.h"
#include "hal/sd.h"
#include "paths.h"
#include "util/log.h"

static const char *TAG = "sprite";

static const char *path_for(SpriteSize s) {
  switch (s) {
    case SpriteSize::S96: return SPRITES_96_PATH;
    case SpriteSize::S64: return SPRITES_64_PATH;
    case SpriteSize::S24: return SPRITES_24_PATH;
    default: return SPRITES_48_PATH;
  }
}

static uint16_t px_for(SpriteSize s) {
  switch (s) {
    case SpriteSize::S96: return 96;
    case SpriteSize::S64: return 64;
    case SpriteSize::S24: return 24;
    default: return 48;
  }
}

// Vaga = um sprite 24 (1152 B): um 48 ocupa 4, um 64 ocupa 8, um 96 ocupa 16.
// Área de 46 KB, como antes (10 × 48).
static constexpr size_t kSlotSize = 24 * 24 * 2;  // 1152 B
static constexpr uint8_t kSlots = 40;
static uint8_t *s_arena = nullptr;
static uint8_t s_used[kSlots];  // 0 = livre; senão, nº de vagas do bloco que começa aqui
static bool s_busy[kSlots];

void sprite_reserve() {
  if (s_arena) return;
  s_arena = (uint8_t *)malloc(kSlotSize * kSlots);
  if (!s_arena) LOG_E(TAG, "sem bloco de %u B pros sprites", (unsigned)(kSlotSize * kSlots));
}

// Primeira sequência de `n` vagas livres; nullptr se não houver.
static uint8_t *arena_take(size_t bytes) {
  if (!s_arena) return nullptr;
  uint8_t n = (uint8_t)((bytes + kSlotSize - 1) / kSlotSize);
  for (uint8_t i = 0; i + n <= kSlots; i++) {
    bool free_run = true;
    for (uint8_t k = 0; k < n; k++) {
      if (s_busy[i + k]) {
        free_run = false;
        break;
      }
    }
    if (!free_run) continue;
    for (uint8_t k = 0; k < n; k++) s_busy[i + k] = true;
    s_used[i] = n;
    return s_arena + i * kSlotSize;
  }
  return nullptr;
}

// true se `p` era da área (e foi devolvido).
static bool arena_give(uint8_t *p) {
  if (!s_arena || p < s_arena || p >= s_arena + kSlotSize * kSlots) return false;
  uint8_t i = (uint8_t)((p - s_arena) / kSlotSize);
  for (uint8_t k = 0; k < s_used[i]; k++) s_busy[i + k] = false;
  s_used[i] = 0;
  return true;
}

void *sprite_arena_borrow(size_t bytes) {
  void *p = arena_take(bytes);
  return p ? p : malloc(bytes);
}

void sprite_arena_return(void *p) {
  if (p && !arena_give((uint8_t *)p)) free(p);
}

// Cabeçalho validado uma vez por arquivo (cache por tamanho).
static bool s_checked[4];
static uint16_t s_count[4];

bool sprite_load(uint16_t n, SpriteSize size, Sprite *out) {
  // Recarregar no mesmo Sprite libera o anterior (antes, o memset vazava o buffer).
  sprite_free(out);
  const char *path = path_for(size);
  const uint16_t px = px_for(size);
  const int slot = (int)size;

  // O HAL reaproveita o arquivo se já estiver aberto: 10 sprites = 1 abertura.
  size_t file_size = 0;
  if (!sd_open_read(path, &file_size)) return false;

  if (!s_checked[slot]) {
    uint8_t h[SPRITE_HEADER_SIZE];
    SpriteFileHeader hdr;
    if (!sd_read_at(0, h, sizeof(h)) || !sprite_header_decode(h, &hdr) || hdr.px != px ||
        file_size < sprite_record_offset(px, hdr.count + 1)) {
      LOG_E(TAG, "%s: cabeçalho inválido ou arquivo truncado", path);
      return false;
    }
    s_count[slot] = hdr.count;
    s_checked[slot] = true;
  }
  if (n < 1 || n > s_count[slot]) return false;

  const size_t data_size = (size_t)px * px * 2;
  uint8_t *buf = arena_take(data_size);
  if (!buf) buf = (uint8_t *)malloc(data_size);
  if (!buf) {
    LOG_E(TAG, "sem memória para #%u (%u B)", n, (unsigned)data_size);
    return false;
  }
  lv_img_header_t header;
  size_t off = sprite_record_offset(px, n);
  bool ok = sd_read_at(off, (uint8_t *)&header, sizeof(header)) &&
            sd_read_at(off + sizeof(header), buf, data_size);
  if (!ok || header.w != px || header.h != px ||
      header.cf != LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED) {
    LOG_E(TAG, "%s: registro #%u inválido", path, n);
    if (!arena_give(buf)) free(buf);
    return false;
  }

  out->buf = buf;
  out->dsc.header = header;
  out->dsc.data_size = data_size;
  out->dsc.data = buf;
  return true;
}

void sprite_free(Sprite *s) {
  if (s->buf && !arena_give(s->buf)) free(s->buf);
  memset(s, 0, sizeof(*s));
}
