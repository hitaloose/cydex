#include "WifiList.h"

#include <cstring>

int WifiList::find(const char *ssid) const {
  for (uint8_t i = 0; i < count; i++) {
    if (strcmp(items[i].ssid, ssid) == 0) return i;
  }
  return -1;
}

void WifiList::add(const char *ssid, int8_t rssi, bool secure) {
  if (!ssid || !ssid[0]) return;
  int i = find(ssid);
  if (i >= 0) {
    if (rssi > items[i].rssi) {
      items[i].rssi = rssi;
      items[i].secure = secure;
    }
    return;
  }
  WifiNet *slot = nullptr;
  if (count < WIFI_LIST_MAX) {
    slot = &items[count++];
  } else {
    uint8_t weakest = 0;
    for (uint8_t k = 1; k < count; k++) {
      if (items[k].rssi < items[weakest].rssi) weakest = k;
    }
    if (items[weakest].rssi >= rssi) return;
    slot = &items[weakest];
  }
  strncpy(slot->ssid, ssid, WIFI_SSID_MAX);
  slot->ssid[WIFI_SSID_MAX] = '\0';
  slot->rssi = rssi;
  slot->secure = secure;
}

void WifiList::sort(const char *current) {
  auto rank = [&](const WifiNet &n) {
    bool cur = current && current[0] && strcmp(n.ssid, current) == 0;
    return (cur ? 1000 : 0) + n.rssi;
  };
  // Inserção: no máximo 20 itens.
  for (uint8_t i = 1; i < count; i++) {
    WifiNet v = items[i];
    int r = rank(v);
    int j = i - 1;
    while (j >= 0 && rank(items[j]) < r) {
      items[j + 1] = items[j];
      j--;
    }
    items[j + 1] = v;
  }
}

uint8_t wifi_bars(int8_t rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -78) return 2;
  return 1;
}
