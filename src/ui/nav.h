#pragma once

#include <cstdint>

// Telas do canvas. Uma por vez na memória: a anterior é apagada ao trocar
// (sem PSRAM não dá pra manter todas vivas). Troca sem animação.
enum class Screen : uint8_t {
  Boot,
  SdError,
  Menu,
  List,
  Grid,
  GoTo,
  Search,
  Types,
  Progress,
  Settings,
  SettingsShowcase,
  SettingsSystem,
  SettingsWifi,
  WifiInfo,
  WifiPassword,
  Detail,
  Showcase,
};

void nav_go(Screen s);
// Remonta a tela atual (ex.: depois de trocar o idioma).
void nav_reload();
Screen nav_current();
// Tela de onde se veio (Busca e Tipos voltam pra ela).
Screen nav_previous();
