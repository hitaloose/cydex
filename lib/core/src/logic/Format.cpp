#include "Format.h"

#include <cstdio>

void format_tenths(uint16_t tenths, const char *sep, char *out, size_t size) {
  snprintf(out, size, "%u%s%u", (unsigned)(tenths / 10), sep, (unsigned)(tenths % 10));
}
