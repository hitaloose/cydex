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
    default: return SPRITES_48_PATH;
  }
}

static uint16_t px_for(SpriteSize s) {
  switch (s) {
    case SpriteSize::S96: return 96;
    case SpriteSize::S64: return 64;
    default: return 48;
  }
}

// Cabeçalho validado uma vez por arquivo (cache por tamanho).
static bool s_checked[3];
static uint16_t s_count[3];

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
  uint8_t *buf = (uint8_t *)malloc(data_size);
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
    free(buf);
    return false;
  }

  out->buf = buf;
  out->dsc.header = header;
  out->dsc.data_size = data_size;
  out->dsc.data = buf;
  return true;
}

void sprite_free(Sprite *s) {
  if (s->buf) free(s->buf);
  memset(s, 0, sizeof(*s));
}
