#pragma once

#include <cstdint>

// Struct de dados puro — sem I/O. Detecção de versão fica em
// services/save/gen3/Gen3SaveParser.
enum class Gen3GameVersion : uint8_t {
  Unknown,
  RubySapphire,
  Emerald,
  FireRedLeafGreen,
};
