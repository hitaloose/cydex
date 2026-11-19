#pragma once

#include <cstdint>

#include "Gen3SaveParser.h"

// Consulta bits de "visto"/"capturado" da Pokedex de um save já parseado.
// `national_dex_number` vai de 1 (Bulbasaur) até GEN3_NATIONAL_DEX_SPECIES_COUNT
// (386, Deoxys) — o bitfield do Gen3 já usa a ordem da dex nacional
// diretamente (confirmado no PKHeX: SAV3.GetCaught/GetSeen fazem
// `bit = species - 1` sem remapear), diferente do campo de espécie de um
// Pokémon individual (que usa ordem interna do Gen3 — só relevante pro
// Gen3PartyParser/Gen3BoxParser, ainda stub).
bool gen3_pokedex_is_caught(const Gen3ParsedSave &save, uint16_t national_dex_number);
bool gen3_pokedex_is_seen(const Gen3ParsedSave &save, uint16_t national_dex_number);

uint16_t gen3_pokedex_count_caught(const Gen3ParsedSave &save);
uint16_t gen3_pokedex_count_seen(const Gen3ParsedSave &save);

// Copia o bitfield "capturado" inteiro (GEN3_POKEDEX_BITMASK_BYTES bytes, 1
// bit por espécie da dex nacional 1..GEN3_NATIONAL_DEX_SPECIES_COUNT, mesmo
// layout de gen3_pokedex_is_caught) pra `out` — usado pra agregar o status
// de vários saves sem precisar manter o Gen3ParsedSave inteiro (~19.8KB) em
// memória depois do parsing (ver services/PokedexService).
void gen3_pokedex_copy_caught_bitmask(const Gen3ParsedSave &save, uint8_t *out);
