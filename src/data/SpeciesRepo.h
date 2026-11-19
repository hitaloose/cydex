#pragma once

#include <cstdint>

#include "dex/SpeciesRecord.h"

// Catálogo lido de /cydex/species.bin. Nomes (~16 KB) e tipos (2 KB) ficam em
// RAM, pra Lista, Busca e filtro de tipo. O registro completo é lido do SD
// quando a ficha abre.

// `done`/`total` = registros lidos. Usado pela barra do Boot ("642 / 1025").
typedef void (*SpeciesProgressCb)(uint16_t done, uint16_t total, void *user);

// Reserva a tabela de nomes (16 KB, util/early_alloc). Chamar no setup,
// depois de early_alloc_init().
bool species_repo_reserve();
bool species_repo_init(SpeciesProgressCb cb, void *user);
uint16_t species_repo_count();
// Nome UTF-8 de `n` (1..count), ou "" fora da faixa.
const char *species_repo_name(uint16_t n);
// Tipos de `n` (SpeciesType::None fora da faixa). Assinatura compatível com
// TypeLookup (logic/Catalog.h).
void species_repo_types(uint16_t n, SpeciesType *t1, SpeciesType *t2);
// Lê o registro completo do SD.
bool species_repo_get(uint16_t n, SpeciesRecord *out);
