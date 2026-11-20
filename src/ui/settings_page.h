#pragma once

#include <lvgl.h>

#include <cstdint>

// Corpo paginado das telas de Ajustes (05, 05a, 05b, 05c, 05f, 05g; nota
// "Ajustes: regras" do canvas). Abaixo do cabeçalho, linhas empilhadas (34 px,
// algumas maiores). Se tudo couber nos 208 px, uma página e sem rodapé; senão
// as páginas usam 170 px e o rodapé ◀ ▶ (pager_bar) aparece embaixo, com só
// "pág X de Y" no meio (sem Ir para). Trocar de página remonta só as linhas.

constexpr lv_coord_t SETTINGS_ROW_H = 34;

struct SettingsRow {
  lv_coord_t h;
  // Monta a linha `i` dentro de `body`, na altura `y` (relativa ao corpo).
  // Em geral: set_row(body, y, h) / set_link_row(body, y, h, ...).
  void (*build)(lv_obj_t *body, lv_coord_t y, lv_coord_t h, uint8_t i);
};

// `rows` e `page` precisam viver tanto quanto a tela (estáticos de quem chama);
// `page` guarda a página entre remontagens (ex.: voltar da Escolha). Devolve a
// altura (relativa à tela) logo abaixo da última linha da página atual.
lv_coord_t settings_page(lv_obj_t *scr, const SettingsRow *rows, uint8_t count, uint8_t *page);

// Página em que a linha `row` cai (pra abrir já na opção atual).
uint8_t settings_page_of(const SettingsRow *rows, uint8_t count, uint8_t row);
