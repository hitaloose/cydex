#pragma once

#include <cstddef>
#include <cstdint>

// Campo "Número" da tela Ir para: até 4 dígitos, sem zero à esquerda,
// nunca passa de `max` (dígito que estouraria é ignorado).
struct NumberInput {
  uint16_t value = 0;  // 0 = vazio
  uint16_t max = 1025;

  // true se aceitou o dígito.
  bool press_digit(uint8_t d);
  void backspace();
  void clear() { value = 0; }
  bool empty() const { return value == 0; }
  // "#025" ou "#___" quando vazio.
  void format(char *out, size_t size) const;
};
