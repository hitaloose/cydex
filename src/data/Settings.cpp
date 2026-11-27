#include "Settings.h"

#include <Preferences.h>
#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "logic/Showcase.h"
#include "util/log.h"

// Idioma de fábrica, trocável por build flag: -D CYDEX_DEFAULT_LANG=\"en\"
#ifndef CYDEX_DEFAULT_LANG
#define CYDEX_DEFAULT_LANG "pt-BR"
#endif

static const char *TAG = "settings";
static const char *NS = "cydex";
static const char *KEY_LANG = "lang";
// Chaves da NVS: no máximo 15 caracteres.
static const char *KEY_BRIGHT = "bright";
static const char *KEY_SOUND = "sound";
static const char *KEY_GRID = "open_grid";
static const char *KEY_OFF = "off_min";
static const char *KEY_SC_INT = "sc_int";
static const char *KEY_SC_RAND = "sc_rand";
static const char *KEY_SC_ALL = "sc_all";  // antiga (bool vistos); lida só se não houver sc_src
static const char *KEY_SC_SRC = "sc_src";
// Brilho próprio da Vitrine (dia/noite): saiu. Apagadas no boot, se existirem.
static const char *KEY_SC_BRIGHT_OLD = "sc_bright";
static const char *KEY_SC_BRIGHT_N_OLD = "sc_bright_n";
static const char *KEY_SC_BOOT = "sc_boot";
static const char *KEY_BRIGHT_N = "bright_n";
static const char *KEY_LOC_LAT = "loc_lat";  // ausente = sem localidade
static const char *KEY_LOC_LON = "loc_lon";
static const char *KEY_LOC_TZ = "loc_tz";
static const char *KEY_WIFI_ON = "wifi_on";
static const char *KEY_WIFI_SSID = "wifi_ssid";
static const char *KEY_WIFI_PASS = "wifi_pass";
static const char *KEY_HOOK_KEY = "hook_key";

static AppSettings s_settings;

static uint8_t clamp_pct(uint8_t v, uint8_t lo) { return v < lo ? lo : (v > 100 ? 100 : v); }

void settings_init() {
  Preferences p;
  // Leitura-escrita de propósito: em modo só-leitura o begin() falha (e loga
  // erro) enquanto o namespace não existe, o que acontece até o 1o ajuste salvo.
  p.begin(NS, false);
  // isKey antes de cada get: get de chave ausente loga erro mesmo com padrão.
  String code = p.isKey(KEY_LANG) ? p.getString(KEY_LANG) : String(CYDEX_DEFAULT_LANG);
  AppSettings d;  // padrões
  AppSettings &s = s_settings;
  s.brightness = clamp_pct(p.isKey(KEY_BRIGHT) ? p.getUChar(KEY_BRIGHT) : d.brightness, 10);
  s.sound = p.isKey(KEY_SOUND) ? p.getBool(KEY_SOUND) : d.sound;
  s.open_grid = p.isKey(KEY_GRID) ? p.getBool(KEY_GRID) : d.open_grid;
  uint16_t off = p.isKey(KEY_OFF) ? p.getUShort(KEY_OFF) : d.screen_off_min;
  s.screen_off_min = SCREEN_OFF_MIN[option_index(SCREEN_OFF_MIN, SCREEN_OFF_COUNT, off)];
  uint16_t iv = p.isKey(KEY_SC_INT) ? p.getUShort(KEY_SC_INT) : d.showcase_interval_s;
  s.showcase_interval_s =
      SHOWCASE_INTERVALS_S[option_index(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, iv)];
  s.showcase_random = p.isKey(KEY_SC_RAND) ? p.getBool(KEY_SC_RAND) : d.showcase_random;
  if (p.isKey(KEY_SC_SRC)) s.showcase_source = p.getUChar(KEY_SC_SRC);
  else if (p.isKey(KEY_SC_ALL)) s.showcase_source = p.getBool(KEY_SC_ALL) ? 1 : 0;
  else s.showcase_source = d.showcase_source;
  if (s.showcase_source >= SHOWCASE_SOURCE_COUNT) s.showcase_source = d.showcase_source;
  s.showcase_on_boot = p.isKey(KEY_SC_BOOT) ? p.getBool(KEY_SC_BOOT) : d.showcase_on_boot;
  // Dia e noite seguem nas chaves de antes (o brilho normal de dia e de noite).
  s.brightness_night =
      clamp_pct(p.isKey(KEY_BRIGHT_N) ? p.getUChar(KEY_BRIGHT_N) : d.brightness_night, 5);
  if (p.isKey(KEY_SC_BRIGHT_OLD)) p.remove(KEY_SC_BRIGHT_OLD);
  if (p.isKey(KEY_SC_BRIGHT_N_OLD)) p.remove(KEY_SC_BRIGHT_N_OLD);
  s.has_location = p.isKey(KEY_LOC_LAT) && p.isKey(KEY_LOC_LON);
  if (s.has_location) {
    s.lat = p.getFloat(KEY_LOC_LAT);
    s.lon = p.getFloat(KEY_LOC_LON);
    s.tz_min = p.isKey(KEY_LOC_TZ) ? p.getShort(KEY_LOC_TZ) : 0;
  }
  s.wifi_on = p.isKey(KEY_WIFI_ON) ? p.getBool(KEY_WIFI_ON) : d.wifi_on;
  if (p.isKey(KEY_WIFI_SSID)) p.getString(KEY_WIFI_SSID, s.wifi_ssid, sizeof(s.wifi_ssid));
  if (p.isKey(KEY_WIFI_PASS)) p.getString(KEY_WIFI_PASS, s.wifi_pass, sizeof(s.wifi_pass));
  if (p.isKey(KEY_HOOK_KEY)) p.getString(KEY_HOOK_KEY, s.hook_key, sizeof(s.hook_key));
  if (strlen(s.hook_key) != 8) {
    // 1o boot: chave do hook com 32 bits do RNG de hardware.
    snprintf(s.hook_key, sizeof(s.hook_key), "%08lx", (unsigned long)esp_random());
    p.putString(KEY_HOOK_KEY, s.hook_key);
  }
  p.end();
  i18n_set_lang(i18n_lang_from_code(code.c_str()));
  LOG_I(TAG, "idioma=%s brilho=%u%%/%u%% som=%u grade=%u apagar=%umin vitrine=%us/%s/%u%s",
        i18n_lang_code(i18n_lang()), s.brightness, s.brightness_night, s.sound, s.open_grid,
        s.screen_off_min, s.showcase_interval_s, s.showcase_random ? "aleat" : "seq",
        s.showcase_source, s.showcase_on_boot ? " (abre no boot)" : "");
  LOG_I(TAG, "local=%s%.3f,%.3f tz=%d", s.has_location ? "" : "(nenhuma) ", s.lat, s.lon, s.tz_min);
  LOG_I(TAG, "wifi=%s rede=\"%s\"", s.wifi_on ? "ligado" : "desligado", s.wifi_ssid);
}

AppSettings &settings() { return s_settings; }

void settings_save() {
  const AppSettings &s = s_settings;
  Preferences p;
  p.begin(NS, false);
  // put* só grava na flash se o valor mudou (a NVS compara antes).
  p.putUChar(KEY_BRIGHT, s.brightness);
  p.putBool(KEY_SOUND, s.sound);
  p.putBool(KEY_GRID, s.open_grid);
  p.putUShort(KEY_OFF, s.screen_off_min);
  p.putUShort(KEY_SC_INT, s.showcase_interval_s);
  p.putBool(KEY_SC_RAND, s.showcase_random);
  p.putUChar(KEY_SC_SRC, s.showcase_source);
  p.putBool(KEY_SC_BOOT, s.showcase_on_boot);
  p.putUChar(KEY_BRIGHT_N, s.brightness_night);
  p.end();
}

void settings_save_location() {
  const AppSettings &s = s_settings;
  Preferences p;
  p.begin(NS, false);
  if (s.has_location) {
    p.putFloat(KEY_LOC_LAT, s.lat);
    p.putFloat(KEY_LOC_LON, s.lon);
    p.putShort(KEY_LOC_TZ, s.tz_min);
  } else {
    // isKey antes: remove de chave ausente loga erro.
    if (p.isKey(KEY_LOC_LAT)) p.remove(KEY_LOC_LAT);
    if (p.isKey(KEY_LOC_LON)) p.remove(KEY_LOC_LON);
    if (p.isKey(KEY_LOC_TZ)) p.remove(KEY_LOC_TZ);
  }
  p.end();
}

void settings_save_wifi() {
  const AppSettings &s = s_settings;
  Preferences p;
  p.begin(NS, false);
  p.putBool(KEY_WIFI_ON, s.wifi_on);
  p.putString(KEY_WIFI_SSID, s.wifi_ssid);
  p.putString(KEY_WIFI_PASS, s.wifi_pass);
  p.end();
}

void settings_set_lang(LangId lang) {
  i18n_set_lang(lang);
  Preferences p;
  p.begin(NS, false);
  p.putString(KEY_LANG, i18n_lang_code(lang));
  p.end();
}
