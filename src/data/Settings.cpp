#include "Settings.h"

#include <Preferences.h>

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
static const char *KEY_SC_ALL = "sc_all";
static const char *KEY_SC_BRIGHT = "sc_bright";
static const char *KEY_SC_BOOT = "sc_boot";

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
  s.showcase_all_seen = p.isKey(KEY_SC_ALL) ? p.getBool(KEY_SC_ALL) : d.showcase_all_seen;
  s.showcase_brightness =
      clamp_pct(p.isKey(KEY_SC_BRIGHT) ? p.getUChar(KEY_SC_BRIGHT) : d.showcase_brightness, 5);
  s.showcase_on_boot = p.isKey(KEY_SC_BOOT) ? p.getBool(KEY_SC_BOOT) : d.showcase_on_boot;
  p.end();
  i18n_set_lang(i18n_lang_from_code(code.c_str()));
  LOG_I(TAG, "idioma=%s brilho=%u som=%u grade=%u apagar=%umin vitrine=%us/%s/%s/%u%%%s",
        i18n_lang_code(i18n_lang()), s.brightness, s.sound, s.open_grid, s.screen_off_min,
        s.showcase_interval_s, s.showcase_random ? "aleat" : "seq",
        s.showcase_all_seen ? "vistos" : "capt", s.showcase_brightness,
        s.showcase_on_boot ? " (abre no boot)" : "");
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
  p.putBool(KEY_SC_ALL, s.showcase_all_seen);
  p.putUChar(KEY_SC_BRIGHT, s.showcase_brightness);
  p.putBool(KEY_SC_BOOT, s.showcase_on_boot);
  p.end();
}

void settings_set_lang(LangId lang) {
  i18n_set_lang(lang);
  Preferences p;
  p.begin(NS, false);
  p.putString(KEY_LANG, i18n_lang_code(lang));
  p.end();
}
