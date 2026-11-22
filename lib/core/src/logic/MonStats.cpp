#include "MonStats.h"

static uint32_t cube(uint32_t l) { return l * l * l; }

uint32_t exp_for_level(GrowthRate rate, uint8_t level) {
  if (level <= 1) return 0;
  if (level > 100) level = 100;
  uint32_t l = level;
  uint32_t c = cube(l);
  switch (rate) {
    case GrowthRate::Fast: return c * 4 / 5;
    case GrowthRate::Medium: return c;
    case GrowthRate::Slow: return c * 5 / 4;
    case GrowthRate::MediumSlow: {
      int32_t v = (int32_t)(c * 6 / 5) - 15 * (int32_t)(l * l) + 100 * (int32_t)l - 140;
      return v < 0 ? 0 : (uint32_t)v;
    }
    case GrowthRate::Erratic:
      if (l <= 50) return c * (100 - l) / 50;
      if (l <= 68) return c * (150 - l) / 100;
      if (l <= 98) return c * ((1911 - 10 * l) / 3) / 500;
      return c * (160 - l) / 100;
    case GrowthRate::Fluctuating:
      if (l <= 15) return c * ((l + 1) / 3 + 24) / 50;
      if (l <= 36) return c * (l + 14) / 50;
      return c * (l / 2 + 32) / 50;
    case GrowthRate::Unknown:
    default:
      return 0;
  }
}

uint8_t level_from_exp(GrowthRate rate, uint32_t exp) {
  if (rate == GrowthRate::Unknown) return 0;
  uint8_t lv = 1;
  while (lv < 100 && exp_for_level(rate, lv + 1) <= exp) lv++;
  return lv;
}

void nature_effect(uint8_t nature, uint8_t *up, uint8_t *down) {
  // Ordem da natureza no jogo: Atq, Def, Vel, AtE, DfE.
  static const uint8_t kOrder[5] = {1, 2, 5, 3, 4};
  nature %= 25;
  *up = kOrder[nature / 5];
  *down = kOrder[nature % 5];
}

uint16_t mon_stat(uint8_t index, uint8_t base, uint8_t iv, uint8_t ev, uint8_t level,
                  uint8_t nature) {
  uint32_t core = (2u * base + iv + ev / 4u) * level / 100u;
  if (index == 0) return (uint16_t)(core + level + 10);
  uint32_t v = core + 5;
  uint8_t up, down;
  nature_effect(nature, &up, &down);
  if (up != down) {
    if (index == up) v = v * 110 / 100;
    else if (index == down) v = v * 90 / 100;
  }
  return (uint16_t)v;
}

void mon_fill_stats(SaveMon &m, const uint8_t base[6], GrowthRate rate) {
  if (m.level == 0) m.level = level_from_exp(rate, m.exp);
  if (m.level == 0) return;
  for (uint8_t i = 0; i < SAVE_STAT_COUNT; i++) {
    m.stats[i] = mon_stat(i, base[i], m.ivs[i], m.evs[i], m.level, m.nature);
  }
  if (m.species == 292) m.stats[0] = 1;
  m.hp = m.stats[0];
}
