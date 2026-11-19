#pragma once

#include <cstdint>

// Struct de dados puro — sem I/O. Preenchido por
// services/save/gen3/Gen3TrainerInfo a partir de um save já parseado.
struct Trainer {
  char name[16];  // nome do treinador já decodificado (charset Gen3 -> UTF-8)
  uint16_t tid;
  uint16_t sid;
  uint8_t badges;  // bitmask de 8 bits, 1 bit por badge obtida
  uint16_t played_hours;
  uint8_t played_minutes;
  uint8_t played_seconds;
};
