#include "NumberInput.h"

#include <cstdio>

bool NumberInput::press_digit(uint8_t d) {
  if (d > 9) return false;
  uint32_t next = (uint32_t)value * 10 + d;
  if (next == 0 || next > max) return false;  // "0" sozinho não entra
  value = (uint16_t)next;
  return true;
}

void NumberInput::backspace() { value = (uint16_t)(value / 10); }

void NumberInput::format(char *out, size_t size) const {
  if (value == 0) snprintf(out, size, "#___");
  else snprintf(out, size, "#%03u", value);
}
