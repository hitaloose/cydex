#pragma once

#include <cstddef>
#include <cstdint>

// Busca do catálogo (tela 08). Comparação "dobrada": minúsculas, sem acento
// (Latin-1) e sem pontuação/espaço, então "mrmime" acha "Mr. Mime" e
// "flabebe" acha "Flabébé".

constexpr size_t SEARCH_MAX = 12;  // caracteres digitados

// Dobra `in` (UTF-8) em `out`; retorna o tamanho. Só sobram [a-z0-9].
size_t search_fold(const char *in, char *out, size_t size);

// Consulta já dobrada, pronta pra comparar com cada espécie.
struct SearchKey {
  char folded[SEARCH_MAX + 1];
  uint8_t len;
  bool numeric;  // só dígitos: compara com o número, não com o nome
};

// false se não sobrar nada pra buscar (vazio ou só pontuação).
bool search_key_make(const char *text, SearchKey *out);

// Número: "%03u" contém a consulta (qualquer espécie, "#025 ???" se não vista).
// Nome: só espécie vista (não entrega quem ainda não foi visto).
bool search_matches(const SearchKey &key, uint16_t n, const char *name, bool seen);
