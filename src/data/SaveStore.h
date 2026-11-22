#pragma once

#include <cstdint>

#include "saves/SaveFormat.h"
#include "saves/SavesCache.h"

// Saves pra UI (16–19a, Ficha "Onde está", Vitrine "Meu time"). Tudo que é
// lista vem do /cydex/saves.cache (gravado no boot pelo StatusStore); só o
// time, as caixas e os times do Hall da Fama leem o arquivo do save, aos
// pedaços, quando a tela pede.

#include "data/SaveScratch.h"

// Esquece contagens e o save aberto (o StatusStore chama depois de regravar
// o saves.cache).
void save_store_invalidate();

// Registros no saves.cache (inclui os não reconhecidos). 0 sem cache.
uint8_t save_store_count();
// Quantos foram reconhecidos (os que entram nas listas).
uint8_t save_store_ok_count();
// Registro `i` (0..count-1). `out` grande (~1 KB): usar estático.
bool save_store_entry(uint8_t i, SaveEntry *out);

// Só o começo do resumo (nome, jogo, tempo, contagens): o que as listas
// mostram. false se o registro não existir ou não for reconhecido.
bool save_store_head(uint8_t i, SaveHead *out);
// Vaga `slot` do time do save `i`, do resumo (Vitrine › Meu time).
bool save_store_party_brief(uint8_t i, uint8_t slot, SavePartyBrief *out);
// Criaturas no time de todos os saves reconhecidos.
uint16_t save_store_team_total();
// Nome do arquivo do registro `i` (reconhecido ou não).
bool save_store_file(uint8_t i, char *out, size_t size);

// Abre o save `i` pra leitura aos pedaços. As outras funções reabrem o
// arquivo se outra coisa (sprites, species.bin) usou o SD no meio.
bool save_store_open(uint8_t i);
uint8_t save_store_open_index();
uint8_t save_store_party(SaveMon out[6]);
// Caixa `box`: já com nível e status (base e curva do species.bin).
bool save_store_box(uint8_t box, SaveMon out[SAVE_BOX_SLOTS], char *name, size_t name_size);
uint8_t save_store_hof_team(uint8_t team, SaveMon out[6]);

// "Onde está": onde a espécie `n` aparece em cada save reconhecido.
struct SaveWhere {
  uint8_t save;   // índice no saves.cache
  uint8_t place;  // SAVE_WHERE_PARTY ou nº da caixa (1..)
};
uint8_t save_store_where(uint16_t n, SaveWhere *out, uint8_t max);

// Times do Hall da Fama de todos os saves (ou só de `only_save`, 0xFF =
// todos): por save, na ordem do cache, e dentro dele do mais novo pro mais
// velho. O mesmo time em dois saves (mesmo hash) entra uma vez.
struct HofRef {
  uint8_t save;
  uint8_t team;  // 0 = o mais antigo do save
};
uint16_t save_store_hof_list(HofRef *out, uint16_t max, uint8_t only_save);
// Total de times (com a mesma deduplicação), pro tile do Menu.
uint16_t save_store_hof_total();

// Rótulo do jogo ("Emerald", "R/S", "FR/LG") e a cor da faixa.
const char *save_game_name(SaveGame g);  // "R/S", "FR/LG"
uint32_t save_game_color(SaveGame g);
