#include "Sun.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "net/HttpRequest.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr double kJ2000 = 2451545.0;  // dia juliano de 2000-01-01 12:00 UTC
constexpr double kUnixEpochJd = 2440587.5;

double floor_div(int64_t a, int64_t b) { return std::floor((double)a / (double)b); }

}  // namespace

// https://en.wikipedia.org/wiki/Sunrise_equation (forma completa). Em double:
// o dia juliano (~2,46 milhões) em float perde as horas.
SunDay sun_day(int64_t utc_s, double lat, double lon) {
  // Dia solar local: o meio-dia solar fica ~lon/15 h antes do meio-dia UTC.
  double local_s = (double)utc_s + lon / 360.0 * 86400.0;
  double day = std::floor(local_s / 86400.0);  // dias desde 1970 (local solar)
  double n = day + kUnixEpochJd + 0.5 - kJ2000 + 0.0008;  // dia (meio-dia) desde J2000
  n = std::round(n);
  double jstar = n - lon / 360.0;
  double m = std::fmod(357.5291 + 0.98560028 * jstar, 360.0);
  double mr = m * kDeg;
  double c = 1.9148 * std::sin(mr) + 0.0200 * std::sin(2 * mr) + 0.0003 * std::sin(3 * mr);
  double lambda = std::fmod(m + c + 180.0 + 102.9372, 360.0) * kDeg;
  double transit = kJ2000 + jstar + 0.0053 * std::sin(mr) - 0.0069 * std::sin(2 * lambda);
  double sin_d = std::sin(lambda) * std::sin(23.4397 * kDeg);
  double cos_d = std::cos(std::asin(sin_d));
  double phi = lat * kDeg;
  double cos_w = (std::sin(-0.833 * kDeg) - std::sin(phi) * sin_d) / (std::cos(phi) * cos_d);
  SunDay out{SunKind::Normal, 0, 0};
  if (cos_w < -1.0) {
    out.kind = SunKind::PolarDay;
    return out;
  }
  if (cos_w > 1.0) {
    out.kind = SunKind::PolarNight;
    return out;
  }
  double w = std::acos(cos_w) / kDeg;  // graus
  double rise_jd = transit - w / 360.0;
  double set_jd = transit + w / 360.0;
  out.rise = (int64_t)std::llround((rise_jd - kUnixEpochJd) * 86400.0);
  out.set = (int64_t)std::llround((set_jd - kUnixEpochJd) * 86400.0);
  return out;
}

bool sun_is_night(int64_t utc_s, double lat, double lon) {
  SunDay d = sun_day(utc_s, lat, lon);
  if (d.kind == SunKind::PolarDay) return false;
  if (d.kind == SunKind::PolarNight) return true;
  return utc_s < d.rise || utc_s >= d.set;
}

void sun_format_hhmm(int64_t utc_s, int16_t tz_min, char *out, size_t cap) {
  int64_t local_min = (int64_t)floor_div(utc_s, 60) + tz_min;
  int64_t of_day = ((local_min % 1440) + 1440) % 1440;
  snprintf(out, cap, "%02d:%02d", (int)(of_day / 60), (int)(of_day % 60));
}

static bool parse_number(const char *query, const char *name, double lo, double hi, double *out) {
  char buf[16];
  if (!http_query_param(query, name, buf, sizeof(buf)) || !buf[0]) return false;
  char *end = nullptr;
  double v = strtod(buf, &end);
  if (!end || *end || !std::isfinite(v) || v < lo || v > hi) return false;
  *out = v;
  return true;
}

const char *sun_parse_location(const char *query, float *lat, float *lon, int16_t *tz_min) {
  double la, lo, tz;
  if (!parse_number(query, "lat", -90, 90, &la)) return "lat";
  if (!parse_number(query, "lon", -180, 180, &lo)) return "lon";
  if (!parse_number(query, "tz", -840, 840, &tz) || tz != std::floor(tz)) return "tz";
  *lat = (float)la;
  *lon = (float)lo;
  *tz_min = (int16_t)tz;
  return nullptr;
}
