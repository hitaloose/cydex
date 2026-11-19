#pragma once

#include <cstdint>

// Redes da busca de Wi-Fi (tela 05d), sem depender do Arduino.
constexpr uint8_t WIFI_LIST_MAX = 20;
constexpr uint8_t WIFI_SSID_MAX = 32;

struct WifiNet {
  char ssid[WIFI_SSID_MAX + 1];
  int8_t rssi;
  bool secure;
};

struct WifiList {
  WifiNet items[WIFI_LIST_MAX];
  uint8_t count = 0;

  void clear() { count = 0; }
  // Acrescenta uma rede da busca. SSID vazio (rede oculta) é ignorado; SSID
  // repetido fica só com o sinal mais forte; cheia, troca a mais fraca se a
  // nova for mais forte.
  void add(const char *ssid, int8_t rssi, bool secure);
  // Mais forte primeiro; `current` (rede conectada) vai pro topo.
  void sort(const char *current);
  int find(const char *ssid) const;
};

// RSSI (dBm) -> 1..4 barras.
uint8_t wifi_bars(int8_t rssi);
