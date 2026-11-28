#pragma once

#include <cstddef>
#include <cstdint>

#include "dex/NameTable.h"
#include "saves/SaveSummary.h"

// Nomes de golpes (/cydex/moves.bin) e de itens (/cydex/items_g3.bin e
// items_g4.bin: o índice do item muda de geração pra geração), lidos do SD
// por id quando a ficha do Membro abre. Sem o arquivo ou id além do que ele
// tem (SD antigo), devolvem false e a tela mostra "#id".
bool move_info(uint16_t id, MoveInfo *out);
bool item_name(SaveGame game, uint16_t id, char *out, size_t size);
