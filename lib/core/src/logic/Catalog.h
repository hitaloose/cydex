#pragma once

#include <cstdint>

#include "dex/DexStatus.h"
#include "dex/Types.h"
#include "logic/Search.h"

// Recorte do catálogo que a Lista e a Grade mostram: filtro de status
// (Todos/Capturados/Faltando) + até 2 tipos (tela Tipos) + texto (tela Busca).

enum class StatusFilter : uint8_t { All, Caught, Missing };

struct CatalogQuery {
  StatusFilter status = StatusFilter::All;
  // SpeciesType::None = sem filtro. Com 2 tipos, a espécie precisa ter os dois.
  SpeciesType type_a = SpeciesType::None;
  SpeciesType type_b = SpeciesType::None;
  // Texto do "Ver" da Busca, como digitado ("" = sem busca). Regras em logic/Search.h.
  char text[SEARCH_MAX + 1] = "";

  uint8_t type_count() const;
  bool has_type(SpeciesType t) const;
  // Liga/desliga um tipo (máximo 2). false se já houver 2 e `t` for novo.
  bool toggle_type(SpeciesType t);
  // Como no canvas: com 2 escolhidos, um 3o tipo troca o mais antigo.
  void pick_type(SpeciesType t);
  bool has_text() const { return text[0] != '\0'; }
  // Busca ou tipo ativo: o cabeçalho da Lista/Grade mostra o chip (06c).
  bool refined() const { return has_text() || type_count() > 0; }
  void clear_refinements();
};

// Tipos da espécie `n` (quem chama fornece: no firmware vem do SpeciesRepo).
typedef void (*TypeLookup)(uint16_t n, SpeciesType *t1, SpeciesType *t2);
// Nome da espécie (só usado com busca por texto).
typedef const char *(*NameLookup)(uint16_t n);

bool catalog_matches(const CatalogQuery &q, uint16_t n, const DexStatus &st, TypeLookup types,
                     NameLookup names = nullptr);

// Preenche `out` com os números (1..count) que passam no filtro, em ordem.
// `out` precisa ter `count` posições. Retorna quantos entraram.
uint16_t catalog_build(const CatalogQuery &q, uint16_t count, const DexStatus &st,
                       TypeLookup types, uint16_t *out, NameLookup names = nullptr);

// Índice em `list` do primeiro número >= n (ou o último, se nenhum).
uint16_t catalog_index_at_or_after(const uint16_t *list, uint16_t len, uint16_t n);

// Vizinhos de `n` em `list` (ordenada), pros ‹ › da Ficha: o maior número
// < n e o menor > n. 0 = não há. `n` não precisa estar na lista (ficha aberta
// por um estágio da evolução que o filtro esconde).
void catalog_neighbors(const uint16_t *list, uint16_t len, uint16_t n, uint16_t *prev,
                       uint16_t *next);
