#include "Showcase.h"

const uint16_t SHOWCASE_INTERVALS_S[SHOWCASE_INTERVAL_COUNT] = {30, 60, 120, 300, 900};
const uint16_t SCREEN_OFF_MIN[SCREEN_OFF_COUNT] = {1, 2, 5, 10, 0};

uint8_t option_index(const uint16_t *steps, uint8_t count, uint16_t value) {
  for (uint8_t i = 0; i < count; i++) {
    if (steps[i] == value) return i;
  }
  return 0;
}

uint16_t option_step(const uint16_t *steps, uint8_t count, uint16_t value, int8_t dir) {
  int i = option_index(steps, count, value) + dir;
  if (i < 0) i = 0;
  if (i >= count) i = count - 1;
  return steps[i];
}

uint16_t showcase_next(int32_t current, uint16_t count, bool random, uint32_t rnd) {
  if (count <= 1) return 0;
  if (current < 0 || current >= count) return random ? (uint16_t)(rnd % count) : 0;
  if (!random) return (uint16_t)((current + 1) % count);
  // Pula de 1 a count-1 posições: nunca cai na mesma.
  return (uint16_t)((current + 1 + rnd % (count - 1)) % count);
}

void showcase_shift(uint32_t k, int8_t *dx, int8_t *dy) {
  static const int8_t kShifts[4][2] = {{0, 0}, {2, 1}, {1, 2}, {-1, 1}};
  *dx = kShifts[k % 4][0];
  *dy = kShifts[k % 4][1];
}

uint32_t showcase_cycle_minutes(uint16_t count, uint16_t interval_s) {
  return ((uint32_t)count * interval_s + 59) / 60;
}
