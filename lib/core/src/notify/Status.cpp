#include "Status.h"

#include <cstring>

#include "logic/TextFit.h"

namespace {

constexpr uint32_t kMuted = 0x93A0AD;  // theme::MUTED
constexpr uint32_t kText = 0xECEFF2;   // theme::TEXT

bool same(const StatusItem &a, const StatusItem &b) {
  return strcmp(a.text, b.text) == 0 && a.has_icon == b.has_icon && a.icon == b.icon &&
         a.color == b.color && a.text_color == b.text_color && a.bar == b.bar;
}

}  // namespace

const char *status_error_field(StatusError e) {
  switch (e) {
    case StatusError::Id: return "id";
    case StatusError::Text: return "text";
    case StatusError::Color: return "color";
    case StatusError::TextColor: return "text_color";
    case StatusError::Bar: return "bar";
    case StatusError::Ttl: return "ttl";
    case StatusError::Full: return "full";
    default: return "";
  }
}

StatusError status_from_fields(const NoticeFields &f, const char *id, StatusItem *out) {
  memset(out, 0, sizeof(*out));
  if (!id[0] || !notice_valid_id(id, sizeof(out->id) - 1)) return StatusError::Id;
  memcpy(out->id, id, strlen(id) + 1);

  out->color = kMuted;
  out->text_color = kText;
  if (f.color[0] && !notice_parse_color(f.color, &out->color)) return StatusError::Color;
  if (f.text_color[0] && !notice_parse_color(f.text_color, &out->text_color)) return StatusError::TextColor;
  out->has_icon = f.icon[0] && notice_icon_from_name(f.icon, &out->icon);

  if (f.bar_bad || (f.has_bar && (f.bar < 0 || f.bar > 100))) return StatusError::Bar;
  out->bar = f.has_bar ? (int8_t)f.bar : -1;
  if (f.ttl_bad || (f.has_ttl && (f.ttl < 0 || (uint32_t)f.ttl > STATUS_MAX_TTL_S))) return StatusError::Ttl;
  out->ttl_s = f.has_ttl ? (uint32_t)f.ttl : 0;

  text_fit(f.text, out->text, sizeof(out->text));
  if (!out->text[0]) return StatusError::Text;
  return StatusError::None;
}

void StatusBoard::erase(uint8_t i) {
  for (uint8_t k = i; k + 1 < count_; k++) e_[k] = e_[k + 1];
  count_--;
}

StatusError StatusBoard::put(const StatusItem &it, uint32_t now_ms) {
  for (uint8_t i = 0; i < count_; i++) {
    if (strcmp(e_[i].it.id, it.id) != 0) continue;
    bool changed = !same(e_[i].it, it);
    e_[i].it = it;
    e_[i].set_ms = now_ms;
    if (changed) version_++;
    return StatusError::None;
  }
  if (count_ == STATUS_BOARD_CAP) return StatusError::Full;
  e_[count_].it = it;
  e_[count_].set_ms = now_ms;
  count_++;
  version_++;
  return StatusError::None;
}

uint8_t StatusBoard::remove(const char *id) {
  for (uint8_t i = 0; i < count_; i++) {
    if (strcmp(e_[i].it.id, id) == 0) {
      erase(i);
      version_++;
      return 1;
    }
  }
  return 0;
}

uint8_t StatusBoard::remove_all() {
  uint8_t n = count_;
  count_ = 0;
  if (n) version_++;
  return n;
}

uint8_t StatusBoard::expire(uint32_t now_ms) {
  uint8_t removed = 0;
  for (uint8_t i = 0; i < count_;) {
    uint32_t t = e_[i].it.ttl_s;
    if (t && now_ms - e_[i].set_ms >= t * 1000u) {
      erase(i);
      removed++;
    } else {
      i++;
    }
  }
  if (removed) version_++;
  return removed;
}
