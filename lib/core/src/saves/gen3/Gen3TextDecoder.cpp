#include "Gen3TextDecoder.h"

#include <cstring>

// Tabela G3_EN completa (256 entradas, uma por valor de byte). Símbolos que
// só aparecem em texto de jogo/kana (nunca digitáveis na tela de nome de
// treinador de um cartucho em inglês) viram "?" — não têm como ocorrer de
// verdade num OT name real, então não vale a complexidade de mapear cada um.
static const char *const GEN3_EN_TABLE[256] = {
    // 0x00
    " ", "\xC3\x80", "\xC3\x81", "\xC3\x82", "\xC3\x87", "\xC3\x88", "\xC3\x89", "\xC3\x8A",
    "\xC3\x8B", "\xC3\x8C", "?", "\xC3\x8E", "\xC3\x8F", "\xC3\x92", "\xC3\x93", "\xC3\x94",
    // 0x10
    "\xC5\x92", "\xC3\x99", "\xC3\x9A", "\xC3\x9B", "\xC3\x91", "\xC3\x9F", "\xC3\xA0",
    "\xC3\xA1", "?", "\xC3\xA7", "\xC3\xA8", "\xC3\xA9", "\xC3\xAA", "\xC3\xAB", "\xC3\xAC",
    "?",
    // 0x20
    "\xC3\xAE", "\xC3\xAF", "\xC3\xB2", "\xC3\xB3", "\xC3\xB4", "\xC5\x93", "\xC3\xB9",
    "\xC3\xBA", "\xC3\xBB", "\xC3\xB1", "\xC2\xBA", "\xC2\xAA", "?", "&", "+", "?",
    // 0x30
    "?", "?", "?", "?", "?", "=", ";", "?", "?", "?", "?", "?", "?", "?", "?", "?",
    // 0x40
    "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?",
    // 0x50
    "?", "\xC2\xBF", "\xC2\xA1", "?", "?", "?", "?", "?", "?", "?", "\xC3\x8D", "%", "(", ")",
    "?", "?",
    // 0x60
    "?", "?", "?", "?", "?", "?", "?", "?", "\xC3\xA2", "?", "?", "?", "?", "?", "?",
    "\xC3\xAD",
    // 0x70
    "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "+", "?", "?", "?",
    // 0x80
    "?", "?", "?", "?", "?", "<", ">", "?", "?", "?", "?", "?", "?", "?", "?", "?",
    // 0x90
    "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?",
    // 0xA0
    "?", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "!", "?", ".", "-", ".",
    // 0xB0
    "?", "\"", "\"", "'", "'", "\xE2\x99\x82", "\xE2\x99\x80", "$", ",", "?", "/", "A", "B",
    "C", "D", "E",
    // 0xC0
    "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U",
    // 0xD0
    "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",
    // 0xE0
    "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", ">",
    // 0xF0
    ":", "\xC3\x84", "\xC3\x96", "\xC3\x9C", "\xC3\xA4", "\xC3\xB6", "\xC3\xBC", "?", "?", "?",
    "?", "?", "?", "?", "?", "?",
};

void gen3_text_decode(const uint8_t *data, size_t length, char *out, size_t out_size) {
  size_t out_pos = 0;

  for (size_t i = 0; i < length && data[i] != 0xFF; i++) {
    const char *ch = GEN3_EN_TABLE[data[i]];
    size_t ch_len = strlen(ch);
    if (out_pos + ch_len >= out_size) break;  // sem espaço, trunca
    memcpy(out + out_pos, ch, ch_len);
    out_pos += ch_len;
  }

  out[out_pos] = '\0';
}
