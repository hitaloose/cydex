#include "Search.h"

#include <cstdio>
#include <cstring>

namespace {

// U+00C0..U+00FF -> letra base ('\0' = descarta). Maiúsculas e minúsculas
// têm a mesma tabela (a 2a metade repete a 1a com 0xF7 = ÷ no lugar de ×).
const char kLatin1Base[32] = {'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c', 'e', 'e', 'e',
                              'e', 'i', 'i', 'i', 'i', 'd', 'n', 'o', 'o', 'o', 'o',
                              'o', 0,   'o', 'u', 'u', 'u', 'u', 'y', 't', 's'};

}  // namespace

size_t search_fold(const char *in, char *out, size_t size) {
  size_t len = 0;
  if (size == 0) return 0;
  const unsigned char *p = (const unsigned char *)in;
  while (*p && len + 1 < size) {
    unsigned char ch = *p++;
    char base = 0;
    if (ch >= 'A' && ch <= 'Z') base = (char)(ch - 'A' + 'a');
    else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) base = (char)ch;
    else if (ch == 0xC3 && *p >= 0x80 && *p <= 0xBF) {
      // UTF-8 de U+00C0..U+00FF: C3 80..BF.
      unsigned cp = 0xC0u + (*p++ & 0x3Fu);
      base = kLatin1Base[(cp - 0xC0u) & 0x1Fu];
      if (cp == 0xF7) base = 0;  // ÷
      if (cp == 0xFF) base = 'y';
    } else if (ch >= 0x80) {
      while ((*p & 0xC0) == 0x80) p++;  // outro caractere multibyte: descarta
    }
    if (base) out[len++] = base;
  }
  out[len] = '\0';
  return len;
}

bool search_key_make(const char *text, SearchKey *out) {
  out->len = (uint8_t)search_fold(text, out->folded, sizeof(out->folded));
  out->numeric = out->len > 0;
  for (uint8_t i = 0; i < out->len; i++) {
    if (out->folded[i] < '0' || out->folded[i] > '9') out->numeric = false;
  }
  return out->len > 0;
}

bool search_matches(const SearchKey &key, uint16_t n, const char *name, bool seen) {
  if (key.len == 0) return true;
  if (key.numeric) {
    char num[8];
    snprintf(num, sizeof(num), "%03u", (unsigned)n);
    return strstr(num, key.folded) != nullptr;
  }
  if (!seen || !name) return false;
  char folded[32];
  search_fold(name, folded, sizeof(folded));
  return strstr(folded, key.folded) != nullptr;
}
