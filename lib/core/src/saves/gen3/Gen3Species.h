#pragma once

#include <cstdint>

// O Gen3 guarda a espécie de cada criatura (time, caixas, Hall da Fama) no
// índice interno do jogo, não no nº nacional: 1..251 batem, 252..276 são
// vagas vazias e 277..411 são as espécies de Hoenn numa ordem própria.
// (Os bits da dex do Small block já vêm na ordem nacional.)
// Tabela gerada pelos game_indices da PokeAPI (iguais em R/S/E/FR/LG).

// 0 se o índice não for de espécie nenhuma.
uint16_t gen3_species_to_national(uint16_t internal);
