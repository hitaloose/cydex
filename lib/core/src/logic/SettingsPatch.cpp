#include "SettingsPatch.h"

#include <cstring>

#include "i18n/i18n.h"
#include "logic/Showcase.h"

namespace {

bool parse_uint(const char *v, uint32_t max, uint32_t *out) {
  if (!*v) return false;
  uint32_t n = 0;
  for (; *v; ++v) {
    if (*v < '0' || *v > '9') return false;
    n = n * 10 + (uint32_t)(*v - '0');
    if (n > max) return false;
  }
  *out = n;
  return true;
}

bool parse_bool(const char *v, bool *out) {
  if (!strcmp(v, "1") || !strcmp(v, "true")) *out = true;
  else if (!strcmp(v, "0") || !strcmp(v, "false")) *out = false;
  else return false;
  return true;
}

bool in_steps(const uint16_t *steps, uint8_t count, uint32_t v) {
  for (uint8_t i = 0; i < count; i++) {
    if (steps[i] == v) return true;
  }
  return false;
}

// nullptr = ok; senão o nome do campo (estático, pro 400).
const char *apply(const char *key, const char *v, SettingsPatch *p) {
  uint32_t n;
  if (!strcmp(key, "bright")) {
    if (!parse_uint(v, 100, &n) || n < 10) return "bright";
    p->has_bright = true;
    p->bright = (uint8_t)n;
  } else if (!strcmp(key, "bright_n")) {
    if (!parse_uint(v, 100, &n) || n < 5) return "bright_n";
    p->has_bright_n = true;
    p->bright_n = (uint8_t)n;
  } else if (!strcmp(key, "off_min")) {
    if (!parse_uint(v, 65535, &n) || !in_steps(SCREEN_OFF_MIN, SCREEN_OFF_COUNT, n)) return "off_min";
    p->has_off = true;
    p->off_min = (uint16_t)n;
  } else if (!strcmp(key, "sc_int")) {
    if (!parse_uint(v, 65535, &n) || !in_steps(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, n))
      return "sc_int";
    p->has_sc_int = true;
    p->sc_int = (uint16_t)n;
  } else if (!strcmp(key, "sc_src")) {
    if (!parse_uint(v, SHOWCASE_SOURCE_COUNT - 1, &n)) return "sc_src";
    p->has_sc_src = true;
    p->sc_src = (uint8_t)n;
  } else if (!strcmp(key, "sound")) {
    if (!parse_bool(v, &p->sound)) return "sound";
    p->has_sound = true;
  } else if (!strcmp(key, "grid")) {
    if (!parse_bool(v, &p->grid)) return "grid";
    p->has_grid = true;
  } else if (!strcmp(key, "sc_rand")) {
    if (!parse_bool(v, &p->sc_rand)) return "sc_rand";
    p->has_sc_rand = true;
  } else if (!strcmp(key, "sc_boot")) {
    if (!parse_bool(v, &p->sc_boot)) return "sc_boot";
    p->has_sc_boot = true;
  } else if (!strcmp(key, "lang")) {
    LangId id = i18n_lang_from_code(v);
    // i18n_lang_from_code cai no pt-BR com código desconhecido.
    if (strlen(v) >= sizeof(p->lang) || strcmp(i18n_lang_code(id), v) != 0) return "lang";
    p->has_lang = true;
    strcpy(p->lang, v);
  } else {
    return nullptr;  // desconhecido: quem chama devolve o nome
  }
  return "";
}

}  // namespace

const char *settings_patch_parse(const char *query, SettingsPatch *out) {
  *out = SettingsPatch();
  if (!query || !*query) return "query";
  static char bad[12];
  const char *p = query;
  while (*p) {
    const char *end = strchr(p, '&');
    if (!end) end = p + strlen(p);
    const char *eq = (const char *)memchr(p, '=', (size_t)(end - p));
    char key[12], val[12];
    size_t klen = eq ? (size_t)(eq - p) : 0;
    size_t vlen = eq ? (size_t)(end - eq - 1) : 0;
    if (!eq || klen == 0 || klen >= sizeof(key)) return "query";
    memcpy(key, p, klen);
    key[klen] = '\0';
    if (vlen >= sizeof(val)) {
      // Valor comprido demais: inválido pra qualquer campo conhecido.
      strcpy(bad, key);
      return bad;
    }
    memcpy(val, eq + 1, vlen);
    val[vlen] = '\0';
    const char *err = apply(key, val, out);
    if (!err) {
      strcpy(bad, key);  // campo desconhecido
      return bad;
    }
    if (*err) return err;
    p = *end ? end + 1 : end;
  }
  return out->empty() ? "query" : nullptr;
}
