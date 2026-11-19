#pragma once

#include <cstddef>
#include <cstdint>

// Valor em décimos com 1 casa e o separador do idioma: 7 -> "0,7", 1000 -> "100,0".
// Usado pra altura (dm -> m) e peso (hg -> kg) na Ficha e na Vitrine.
void format_tenths(uint16_t tenths, const char *sep, char *out, size_t size);
