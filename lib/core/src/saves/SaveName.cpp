#include "SaveName.h"

#include <cstring>
#include <strings.h>

static int hex_digit(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

static bool allowed(unsigned char ch) {
  if (ch < 0x20 || ch > 0x7E) return false;
  return !strchr("/\\:*?\"<>|", ch);
}

bool save_name_decode(const char *in, char *out, size_t cap) {
  if (!in || !out || cap == 0) return false;
  size_t n = 0;
  for (const char *p = in; *p; p++) {
    char ch = *p;
    if (ch == '+') {
      ch = ' ';
    } else if (ch == '%') {
      if (hex_digit(p[1]) < 0 || hex_digit(p[2]) < 0) return false;
      ch = (char)(hex_digit(p[1]) * 16 + hex_digit(p[2]));
      p += 2;
    }
    if (!allowed((unsigned char)ch)) return false;
    if (n + 1 >= cap || n >= SAVE_NAME_MAX) return false;
    out[n++] = ch;
  }
  out[n] = '\0';
  if (n < 5 || out[0] == '.' || out[0] == ' ') return false;
  const char *ext = out + n - 4;
  return strcasecmp(ext, ".sav") == 0 || strcasecmp(ext, ".srm") == 0;
}
