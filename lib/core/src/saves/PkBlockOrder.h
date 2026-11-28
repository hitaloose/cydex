#pragma once

#include <cstdint>

// PokeCrypto.BlockPosition (PKHeX), a mesma tabela no Gen3 e no Gen4: linha =
// a ordem sorteada (Gen3: PID % 24; Gen4: ((PID >> 13) & 31) % 24); posição i
// (0..3 = subestrutura A, B, C, D) = em qual vaga guardada ela está.
constexpr uint8_t kPkBlockPos[24][4] = {
    {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 3, 1, 2}, {0, 2, 3, 1}, {0, 3, 2, 1},
    {1, 0, 2, 3}, {1, 0, 3, 2}, {2, 0, 1, 3}, {3, 0, 1, 2}, {2, 0, 3, 1}, {3, 0, 2, 1},
    {1, 2, 0, 3}, {1, 3, 0, 2}, {2, 1, 0, 3}, {3, 1, 0, 2}, {2, 3, 0, 1}, {3, 2, 0, 1},
    {1, 2, 3, 0}, {1, 3, 2, 0}, {2, 1, 3, 0}, {3, 1, 2, 0}, {2, 3, 1, 0}, {3, 2, 1, 0},
};
