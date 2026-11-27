#pragma once

#include <cstdint>

// PUT /settings?<campo>=<valor>[&…] (api 6): o que o app do computador quer
// mudar nos Ajustes. Os campos são os nomes das chaves da NVS (data/Settings)
// e a validação é a mesma da UI:
//   bright    10..100        brilho de dia (%)
//   bright_n  5..100         brilho de noite (%)
//   off_min   SCREEN_OFF_MIN (1, 2, 5, 10, 0 = nunca)
//   sound     0/1            som ao tocar
//   grid      0/1            catálogo abre em Grade
//   sc_int    SHOWCASE_INTERVALS_S (30, 60, 120, 300, 900)
//   sc_rand   0/1            ordem aleatória
//   sc_src    0..3           ShowcaseSource (capturados, vistos, todos, meu time)
//   sc_boot   0/1            ao ligar abre a Vitrine
//   lang      código de um idioma do firmware ("pt-BR", "en")
// Tudo ou nada: um campo inválido ou desconhecido recusa o pedido inteiro.
struct SettingsPatch {
  bool has_bright = false, has_bright_n = false, has_off = false, has_sound = false,
       has_grid = false, has_sc_int = false, has_sc_rand = false, has_sc_src = false,
       has_sc_boot = false, has_lang = false;
  uint8_t bright = 0, bright_n = 0, sc_src = 0;
  uint16_t off_min = 0, sc_int = 0;
  bool sound = false, grid = false, sc_rand = false, sc_boot = false;
  char lang[8] = "";

  bool empty() const {
    return !(has_bright || has_bright_n || has_off || has_sound || has_grid || has_sc_int ||
             has_sc_rand || has_sc_src || has_sc_boot || has_lang);
  }
};

// nullptr = ok; senão o campo do 400 (`{"error":"<campo>"}`): o nome do campo
// inválido ou desconhecido, ou "query" se veio vazia ou malformada.
const char *settings_patch_parse(const char *query, SettingsPatch *out);
