#pragma once

#include <lvgl.h>

// Peças pequenas reaproveitadas pelas telas. Tudo sem animação e sem
// scroll (CLAUDE.md, "Regras de UI").

// Tela vazia com o fundo do tema.
lv_obj_t *ui_screen();
// Container "cru": sem estilo, sem scroll, sem clique.
lv_obj_t *ui_box(lv_obj_t *parent);
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text);
// Ícone A8 (src/ui/icons) pintado com `color`.
lv_obj_t *ui_icon(lv_obj_t *parent, const lv_img_dsc_t *icon, uint32_t color);
// Logo de 3 barras (accent / text / muted), `size` px de lado.
lv_obj_t *ui_logo(lv_obj_t *parent, lv_coord_t size);
// Barra de progresso fina (trilho surface-2, preenchimento accent).
lv_obj_t *ui_progress(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);
void ui_progress_set(lv_obj_t *bar, int32_t value, int32_t max);
// Torna `obj` clicável, chamando `cb` no clique (com `user` em event user_data).
void ui_on_click(lv_obj_t *obj, lv_event_cb_t cb, void *user = nullptr);

// Cabeçalho padrão (canvas): 32 px, voltar 40x32 + título 14/700.
// Retorna o container (flex row); botões à direita entram com ui_header_button.
// title = nullptr: sem título (quem chama põe outra coisa no lugar).
lv_obj_t *ui_header(lv_obj_t *screen, const char *title, lv_event_cb_t on_back,
                    void *back_user = nullptr);
// Botão de ícone 40x32 no cabeçalho.
lv_obj_t *ui_header_button(lv_obj_t *header, const lv_img_dsc_t *icon, lv_event_cb_t cb,
                           void *user = nullptr);
// Empurra os próximos itens do cabeçalho pra direita.
void ui_header_spacer(lv_obj_t *header);

// Marca de status (14 ou 12 px): círculo accent com check (capturado), anel
// muted (visto) ou anel tracejado faint (desconhecido). Ver CLAUDE.md.
enum class SpeciesState : uint8_t;
lv_obj_t *ui_status_mark(lv_obj_t *parent, lv_coord_t size = 14);
void ui_status_mark_set(lv_obj_t *mark, SpeciesState st);

// Chip arredondado de 20 px (tipos da Ficha e da Vitrine).
lv_obj_t *ui_chip(lv_obj_t *parent, const char *text, uint32_t bg, uint32_t fg,
                  lv_coord_t pad_hor = 8);
