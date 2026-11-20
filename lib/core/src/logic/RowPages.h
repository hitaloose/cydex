#pragma once

#include <cstdint>

// Quebra de linhas de altura variável em páginas (Ajustes, Escolha). Sem scroll:
// o que não cabe vai pra próxima página, e aí o rodapé ◀ ▶ ocupa o fim da tela.
//
// `full_h` = altura livre sem rodapé; `paged_h` = altura livre com rodapé.
// Se tudo cabe em `full_h`, é uma página só (sem rodapé). Senão as páginas são
// enchidas em ordem com até `paged_h` cada. Uma linha maior que `paged_h`
// fica sozinha na página dela.
struct RowPages {
  static constexpr uint8_t MAX_PAGES = 16;
  uint8_t count = 0;               // páginas (>= 1, mesmo sem linhas)
  uint8_t start[MAX_PAGES + 1] = {};  // 1a linha de cada página; start[count] = n

  bool paged() const { return count > 1; }
  uint8_t first(uint8_t page) const { return start[page]; }
  uint8_t end(uint8_t page) const { return start[page + 1]; }
  // Página que contém a linha `row`.
  uint8_t page_of(uint8_t row) const;
};

RowPages row_pages(const uint16_t *heights, uint8_t n, uint16_t full_h, uint16_t paged_h);
