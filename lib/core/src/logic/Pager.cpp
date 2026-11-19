#include "Pager.h"

uint16_t Pager::page_count() const {
  if (total == 0 || per_page == 0) return 1;
  return (uint16_t)((total + per_page - 1) / per_page);
}

uint16_t Pager::first_index() const { return (uint16_t)(page * per_page); }

uint16_t Pager::items_on_page() const {
  if (total == 0) return 0;
  uint16_t first = first_index();
  uint16_t left = (uint16_t)(total - first);
  return left < per_page ? left : per_page;
}

bool Pager::prev() {
  if (!has_prev()) return false;
  page--;
  return true;
}

bool Pager::next() {
  if (!has_next()) return false;
  page++;
  return true;
}

bool Pager::go_to_page(uint16_t p) {
  uint16_t last = (uint16_t)(page_count() - 1);
  if (p > last) p = last;
  if (p == page) return false;
  page = p;
  return true;
}

bool Pager::go_to_index(uint16_t index) {
  if (per_page == 0) return false;
  return go_to_page((uint16_t)(index / per_page));
}

void Pager::reset(uint16_t total_items, uint8_t items_per_page) {
  total = total_items;
  per_page = items_per_page ? items_per_page : 1;
  page = 0;
}

void HoldRepeat::press(uint32_t now) {
  active = true;
  steps = 1;
  next_at = now + PAGER_REPEAT_FIRST_MS;
}

bool HoldRepeat::poll(uint32_t now) {
  if (!active) return false;
  if ((int32_t)(now - next_at) < 0) return false;
  steps++;
  next_at = now + (steps >= PAGER_REPEAT_ACCEL_AFTER ? PAGER_REPEAT_FAST_MS : PAGER_REPEAT_SLOW_MS);
  return true;
}
