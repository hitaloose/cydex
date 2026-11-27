#include "WakeLease.h"

#include <cstring>

#include "net/HttpRequest.h"

bool WakeLease::renew(uint32_t now_ms, uint32_t ttl_s) {
  bool fresh = !active_;
  active_ = true;
  start_ = now_ms;
  ttl_ms_ = ttl_s * 1000u;
  return fresh;
}

bool WakeLease::expired(uint32_t now_ms) {
  if (!active_ || now_ms - start_ < ttl_ms_) return false;
  active_ = false;
  return true;
}

uint32_t WakeLease::left_ms(uint32_t now_ms) const {
  if (!active_) return 0;
  uint32_t gone = now_ms - start_;
  return gone >= ttl_ms_ ? 0 : ttl_ms_ - gone;
}

const char *wake_lease_parse(const char *query, uint32_t *ttl_s) {
  *ttl_s = 0;
  if (!query || !*query) return nullptr;
  char buf[8];
  if (!http_query_param(query, "ttl", buf, sizeof(buf))) {
    // Sem o campo: /wake comum. Campo que não coube no buffer: inválido.
    return std::strstr(query, "ttl=") ? "ttl" : nullptr;
  }
  if (!buf[0]) return "ttl";
  uint32_t v = 0;
  for (const char *p = buf; *p; ++p) {
    if (*p < '0' || *p > '9') return "ttl";
    v = v * 10 + (uint32_t)(*p - '0');
    if (v > WakeLease::kMaxTtl) return "ttl";
  }
  *ttl_s = v;
  return nullptr;
}
