#pragma once

#include <cstddef>
#include <cstdint>

#include "dex/NameTable.h"

// Nomes de golpes (/cydex/moves.bin) e de itens do Gen3 (/cydex/items_g3.bin),
// lidos do SD por id quando a ficha do Membro abre. Sem o arquivo (SD antigo),
// devolvem false e a tela mostra "#id".
bool move_info(uint16_t id, MoveInfo *out);
bool item_name_g3(uint16_t id, char *out, size_t size);
