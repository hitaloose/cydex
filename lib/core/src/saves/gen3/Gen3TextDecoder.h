#pragma once

#include <cstddef>
#include <cstdint>

// Decodifica texto do charset proprietário da Gen3 (tabela em inglês —
// jogos japoneses usam uma tabela diferente, fora do escopo da v1) pra
// UTF-8. Para no byte terminador (0xFF) ou no fim de `length`, o que vier
// primeiro. `out` sempre termina em '\0'; trunca silenciosamente se não
// couber em `out_size`. Tabela conferida no código-fonte do PKHeX
// (StringConverter3.G3_EN).
void gen3_text_decode(const uint8_t *data, size_t length, char *out, size_t out_size);
