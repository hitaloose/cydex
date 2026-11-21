#pragma once

#include <cstddef>
#include <cstdint>

// Nascer e pôr do sol (brilho de dia/noite, ui/power). Equação do nascer do
// sol (NOAA simplificada), elevação −0,833° (refração + raio do disco). Tudo
// em UTC: só precisa de latitude e longitude; o fuso serve só pra mostrar.
// Erro típico de 1–2 min fora das latitudes extremas.

// Antes disso o relógio não foi acertado pelo NTP (o ESP32 começa em 1970).
constexpr int64_t SUN_VALID_TIME = 1704067200;  // 2024-01-01 00:00 UTC

enum class SunKind : uint8_t { Normal, PolarDay, PolarNight };

struct SunDay {
  SunKind kind;
  int64_t rise;  // UTC em s (só em Normal)
  int64_t set;
};

// O dia solar local que contém `utc_s` (lon: leste positivo).
SunDay sun_day(int64_t utc_s, double lat, double lon);
// Entre o pôr e o nascer do sol? Polar: noite polar = noite.
bool sun_is_night(int64_t utc_s, double lat, double lon);

// "HH:MM" de `utc_s` no fuso `tz_min` (minutos a leste do UTC). cap >= 6.
void sun_format_hhmm(int64_t utc_s, int16_t tz_min, char *out, size_t cap);

// Query do PUT /location: "lat=-23.55&lon=-46.63&tz=-180". Devolve nullptr se
// deu certo, ou o nome do campo que faltou/está fora da faixa (lat ±90,
// lon ±180, tz ±840 min).
const char *sun_parse_location(const char *query, float *lat, float *lon, int16_t *tz_min);
