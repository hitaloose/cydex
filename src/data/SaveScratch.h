#pragma once

#include <cstdint>

// Rascunho dos parsers de save (SaveReader::scratch, SAVE_SCRATCH_SIZE = 4 KB).
// Um só pro firmware inteiro: boot, API (/saves) e telas rodam na mesma task.
// Definido em SaveStore.cpp.
uint8_t *save_scratch();
