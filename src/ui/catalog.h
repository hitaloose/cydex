#pragma once

#include <cstdint>

#include "logic/Catalog.h"
#include "ui/nav.h"

// Estado do catálogo compartilhado entre Lista, Grade, Ir para e Ficha:
// filtro atual, onde o usuário está (âncora) e de qual visão ele veio.
// Vive a execução inteira (as telas são recriadas a cada navegação).
// Aba da Ficha. Fica igual ao andar com ‹ › (só volta pra Info em não visto).
enum class DetailTab : uint8_t { Info, Stats, Evolution };

struct CatalogState {
  CatalogQuery query;
  uint16_t anchor = 1;          // 1o número visível; mantém a posição ao trocar Lista <-> Grade
  Screen view = Screen::List;   // visão a que "voltar" retorna
  uint16_t selected = 0;        // espécie aberta na Ficha
  DetailTab tab = DetailTab::Info;
  // Pra onde o ‹ da Ficha volta quando ela foi aberta de fora do catálogo
  // (Membro, Hall da Fama). Screen::Boot = voltar pra `view`, como sempre.
  Screen back = Screen::Boot;
};

CatalogState &catalog_state();

// Números que passam no filtro atual (refeito só quando o filtro muda).
const uint16_t *catalog_list(uint16_t *len);
// Chamar depois de mudar catalog_state().query ou de recarregar o status.
void catalog_invalidate();

// Cabeçalho da Lista e da Grade: voltar + "Nacional", ou, com busca/tipo
// ativo, o chip da 06c (tocar limpa e volta pra página 1).
#include <lvgl.h>
lv_obj_t *catalog_header(lv_obj_t *scr, lv_event_cb_t on_back);
