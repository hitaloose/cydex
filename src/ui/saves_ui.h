#pragma once

#include <lvgl.h>

#include <cstddef>
#include <cstdint>

#include "data/SaveStore.h"
#include "data/SpriteLoader.h"
#include "ui/nav.h"

// Estado e peças comuns das telas de Saves (16–18) e do Hall da Fama (19,
// 19a). Vive a execução inteira (as telas são recriadas a cada navegação).

enum class SaveTab : uint8_t { Summary, Party, Boxes };
// De onde o Membro (18) foi aberto: decide o ‹ e o ‹ › do cabeçalho.
enum class MemberFrom : uint8_t { Party, Box };

struct SavesState {
  uint8_t list_page = 0;   // 16
  uint8_t save = 0;        // índice no saves.cache
  SaveTab tab = SaveTab::Summary;
  uint8_t box = 0;         // 17b: caixa aberta
  uint8_t box_slot = 0xFF; // 17b: vaga selecionada (0xFF = a 1ª ocupada)
  MemberFrom from = MemberFrom::Party;
  uint8_t member = 0;      // vaga no time ou na caixa
  uint8_t hof_filter = 0xFF;  // 19: save do filtro (0xFF = todos)
  uint16_t hof_page = 0;
  uint16_t hof_index = 0;     // 19a: posição na lista do 19
  uint16_t focus_species = 0; // Ficha › Onde está: selecionar esta espécie na caixa
};

SavesState &saves_state();

// Memória de trabalho das telas de Saves e do Hall da Fama (time e caixa
// decodificados, lista do Hall): emprestada da área dos sprites, que nessas
// telas sobra (Caixas usa 34 de 40 vagas; o Save pede 6). Já vem zerada; a
// tela devolve no LV_EVENT_DELETE (saves_screen_mem_release). Uma por vez.
constexpr size_t SAVES_SCREEN_MEM = 6912;  // 6 vagas de 1152 B
void *saves_screen_mem_take(size_t bytes);
void saves_screen_mem_release();

// "42h 18min"
void format_playtime(uint16_t h, uint8_t m, char *out, size_t size);
// Tempo da 1ª vitória empacotado (h << 16 | min << 8 | s).
void format_packed_time(uint32_t packed, char *out, size_t size);
// 128450 -> "128.450" (separador de milhar do idioma).
void format_thousands(uint32_t v, char *out, size_t size);
// Nome da natureza 0..24 no idioma atual.
const char *nature_name(uint8_t nature);

// Chip do jogo (18 px, cor do jogo, texto escuro): "Emerald", "R/S", "FR/LG".
lv_obj_t *ui_game_chip(lv_obj_t *parent, SaveGame g);
// Insígnias (losangos de 6, 7, 8 ou 10 px), cheias onde o bit de `badges`
// estiver ligado. `total` 8 = uma fileira; 16 (HG/SS) = duas de 8.
lv_obj_t *ui_badges(lv_obj_t *parent, uint16_t badges, uint8_t total, lv_coord_t size, lv_coord_t gap,
                    lv_coord_t row_gap = 0);
// Ícone de gênero do treinador (0 masculino, 1 feminino).
lv_obj_t *ui_gender(lv_obj_t *parent, uint8_t gender, lv_coord_t size);
// Imagem do sprite `n` (ou a caixa tracejada, se não carregar/ovo). O
// Sprite tem que viver tanto quanto a tela (estático, liberado no DELETE).
lv_obj_t *ui_mon_sprite(lv_obj_t *parent, uint16_t n, SpriteSize size, Sprite *slot, bool load);
// Carrega o sprite em `img` (criada com ui_mon_sprite(..., load=false)).
void ui_mon_sprite_load(lv_obj_t *img, uint16_t n, SpriteSize size, Sprite *slot);
// Card do time/Hall: sprite 48, "Nv"/nível e ★ à direita, apelido e, embaixo,
// a barra de PS (hp_max > 0) ou o nome da espécie (sub != nullptr).
struct MonCardSpec {
  uint16_t species;
  const char *nick;
  uint8_t level;
  bool shiny;
  bool egg;
  uint16_t hp, hp_max;   // barra de PS (0 = sem barra)
  const char *sub;       // segunda linha (Hall da Fama)
};
lv_obj_t *ui_mon_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                      const MonCardSpec &m, Sprite *slot, lv_event_cb_t on_tap, void *user);
// Vaga vazia do card (borda, "vazio" opcional).
lv_obj_t *ui_mon_empty(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                       bool dashed);
// Abre a Ficha da espécie `n` (volta pra tela atual).
void open_species_from(uint16_t n, Screen back);
