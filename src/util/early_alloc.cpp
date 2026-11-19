#include "early_alloc.h"

#include <esp_heap_caps.h>

#include <cstdint>
#include <cstdlib>

#include "board_pins.h"
#include "dex/Dex.h"
#include "dex/SpeciesRecord.h"
#include "util/log.h"

// Tamanho = o que os donos pedem (ver display.cpp e SpeciesRepo.cpp) + folga
// de alinhamento. Se um deles crescer, crescer aqui (early_alloc aborta).
static constexpr size_t kDisplay = BOARD_DISPLAY_WIDTH * 24 * 2;
static constexpr size_t kNames = DEX_COUNT * SPECIES_NAME_SIZE;
static constexpr size_t kSize = kDisplay + kNames + 16;

static uint8_t *s_base = nullptr;
static size_t s_used = 0;

void early_alloc_init() {
  if (s_base) return;
  // DMA-capable (o buffer do display pode ir por DMA no futuro); interna.
  s_base = (uint8_t *)heap_caps_malloc(kSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!s_base) {
    LOG_E("early", "sem bloco de %u bytes no heap", (unsigned)kSize);
    abort();
  }
}

void *early_alloc(size_t size) {
  size = (size + 3) & ~(size_t)3;
  if (!s_base || s_used + size > kSize) {
    LOG_E("early", "early_alloc(%u) estourou (%u/%u)", (unsigned)size, (unsigned)s_used,
          (unsigned)kSize);
    abort();
  }
  void *p = s_base + s_used;
  s_used += size;
  return p;
}
