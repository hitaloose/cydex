#include "RowPages.h"

uint8_t RowPages::page_of(uint8_t row) const {
  for (uint8_t p = 0; p + 1 < count; p++) {
    if (row < start[p + 1]) return p;
  }
  return count ? count - 1 : 0;
}

RowPages row_pages(const uint16_t *heights, uint8_t n, uint16_t full_h, uint16_t paged_h) {
  RowPages rp;
  uint32_t total = 0;
  for (uint8_t i = 0; i < n; i++) total += heights[i];
  rp.start[0] = 0;
  if (total <= full_h) {
    rp.count = 1;
    rp.start[1] = n;
    return rp;
  }
  uint8_t pages = 1;
  uint32_t used = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (used > 0 && used + heights[i] > paged_h && pages < RowPages::MAX_PAGES) {
      rp.start[pages++] = i;
      used = 0;
    }
    used += heights[i];
  }
  rp.count = pages;
  rp.start[pages] = n;
  return rp;
}
