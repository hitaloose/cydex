#pragma once

#include <lvgl.h>

// Cada função monta a tela inteira e devolve o objeto de tela. Quem chama é
// nav_go(). Recursos próprios (sprites, timers) são liberados no
// LV_EVENT_DELETE da tela.

lv_obj_t *boot_screen_create();
// O próximo Boot relê os saves ignorando o status.cache (Sistema › Recarregar).
void boot_request_rescan();
lv_obj_t *sd_error_screen_create();
lv_obj_t *menu_screen_create();
lv_obj_t *list_screen_create();
lv_obj_t *grid_screen_create();
lv_obj_t *goto_screen_create();
// Ficha de catalog_state().selected, na aba catalog_state().tab.
lv_obj_t *detail_screen_create();
lv_obj_t *search_screen_create();
lv_obj_t *types_screen_create();
lv_obj_t *progress_screen_create();
lv_obj_t *settings_screen_create();
lv_obj_t *settings_showcase_screen_create();
lv_obj_t *settings_system_screen_create();
lv_obj_t *showcase_screen_create();
// Balões dos avisos do Claude Code sobre a Vitrine (15b): até 3, na ordem dada
// (o mais importante em cima). `on_tap` no toque de um balão, com o índice em
// user_data (o resto da tela continua saindo pro Menu). Devolve o contêiner
// (apagar = tirar os balões); nullptr se a Vitrine não estiver montada.
struct Notice;
lv_obj_t *showcase_show_notices(const Notice *items, uint8_t count, lv_event_cb_t on_tap);
lv_obj_t *settings_wifi_screen_create();
lv_obj_t *wifi_info_screen_create();
lv_obj_t *wifi_password_screen_create();
#include "ui/nav.h"

// Motivo mostrado na tela de erro (definido pelo Boot).
enum class SdErrorReason : uint8_t { NoCard, NoData };
void sd_error_set_reason(SdErrorReason r);
