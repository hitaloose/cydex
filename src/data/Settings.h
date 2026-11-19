#pragma once

#include <cstdint>

#include "i18n/i18n.h"

// Ajustes persistentes (NVS via Preferences, namespace "cydex"). Telas 05,
// 05b e 05c. Valores ficam em RAM; settings_save() grava na NVS (chamar ao
// soltar um slider, não a cada passo: poupa a flash).

struct AppSettings {
  uint8_t brightness = 70;          // 10..100 %
  bool sound = true;                // clique ao tocar (alto-falante no GPIO 26)
  bool open_grid = false;           // tile Lista do Menu abre a Grade
  uint16_t screen_off_min = 2;      // SCREEN_OFF_MIN (logic/Showcase.h); 0 = nunca
  uint16_t showcase_interval_s = 60;  // SHOWCASE_INTERVALS_S
  bool showcase_random = false;
  bool showcase_all_seen = false;   // false = só capturados
  uint8_t showcase_brightness = 35;  // 5..100 %
  bool showcase_on_boot = false;    // Boot vai direto pra Vitrine
};

// Lê a NVS e aplica (idioma). Chamar antes de montar a primeira tela.
void settings_init();
AppSettings &settings();
void settings_save();

// Salva e aplica. A tela atual precisa ser remontada pra mostrar o idioma novo.
void settings_set_lang(LangId lang);
