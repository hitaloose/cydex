#include "NoticeJson.h"

#include <cstdlib>
#include <cstring>

void NoticeJson::reset() {
  memset(&f_, 0, sizeof(f_));
  state_ = State::BeforeRoot;
  string_return_ = State::Key;
  string_is_key_ = false;
  escape_ = false;
  uni_left_ = 0;
  uni_ = 0;
  hi_surrogate_ = 0;
  depth_ = 0;
  in_actions_ = false;
  key_len_ = 0;
  key_[0] = '\0';
  target_ = nullptr;
  target_cap_ = 0;
  target_len_ = 0;
  scalar_len_ = 0;
  scalar_[0] = '\0';
}

void NoticeJson::feed(const char *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (state_ == State::Done || state_ == State::Error) return;
    step(data[i]);
  }
}

static bool is_ws(char ch) { return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t'; }

// Escreve um código no buffer do valor (UTF-8).
void NoticeJson::put_char(uint32_t cp) {
  char buf[4];
  size_t n;
  if (cp < 0x80) {
    buf[0] = (char)cp;
    n = 1;
  } else if (cp < 0x800) {
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    n = 2;
  } else if (cp < 0x10000) {
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    n = 3;
  } else {
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    n = 4;
  }

  if (string_is_key_) {
    if (key_len_ + n < sizeof(key_)) {
      memcpy(key_ + key_len_, buf, n);
      key_len_ += (uint8_t)n;
      key_[key_len_] = '\0';
    }
    return;
  }
  if (!target_) return;
  // Começo: para quando enche, sem cortar caractere.
  if (target_len_ + n < target_cap_) {
    memcpy(target_ + target_len_, buf, n);
    target_len_ += n;
    target_[target_len_] = '\0';
  } else {
    target_ = nullptr;  // cheio: nada mais entra neste valor
  }
}

// Byte UTF-8 cru (sem escape): passa como está.
void NoticeJson::put_byte(char b) {
  if (string_is_key_) {
    if (key_len_ + 1 < sizeof(key_)) {
      key_[key_len_++] = b;
      key_[key_len_] = '\0';
    }
    return;
  }
  if (!target_) return;
  if (target_len_ + 1 < target_cap_) {
    target_[target_len_++] = b;
    target_[target_len_] = '\0';
  } else {
    // Encheu. Se o byte que não coube é de continuação, o caractere atual
    // ficou pela metade: tira os bytes dele que já entraram.
    if (((unsigned char)b & 0xC0) == 0x80) {
      while (target_len_ > 0 && ((unsigned char)target_[target_len_ - 1] & 0xC0) == 0x80) target_len_--;
      if (target_len_ > 0) target_len_--;  // o byte inicial
      target_[target_len_] = '\0';
    }
    target_ = nullptr;  // nada mais entra neste valor
  }
}

void NoticeJson::end_string() {
  if (string_is_key_) {
    state_ = State::Colon;
    return;
  }
  target_ = nullptr;
  state_ = string_return_;
}

// Escalar de 1o nível terminou: guarda se for uma das chaves conhecidas.
// null conta como ausente.
void NoticeJson::end_scalar() {
  bool too_long = scalar_len_ >= sizeof(scalar_);
  if (!too_long && strcmp(scalar_, "null") == 0) return;
  bool is_true = !too_long && strcmp(scalar_, "true") == 0;
  bool is_false = !too_long && strcmp(scalar_, "false") == 0;
  struct IntSlot {
    const char *key;
    bool *has, *bad;
    int32_t *v;
  };
  const IntSlot ints[] = {
      {"timeout", &f_.has_timeout, &f_.timeout_bad, &f_.timeout},
      {"bar", &f_.has_bar, &f_.bar_bad, &f_.bar},
      {"ttl", &f_.has_ttl, &f_.ttl_bad, &f_.ttl},
  };
  for (const IntSlot &s : ints) {
    if (strcmp(key_, s.key) != 0) continue;
    *s.has = true;
    char *end = nullptr;
    long v = too_long ? 0 : strtol(scalar_, &end, 10);
    if (too_long || end == scalar_ || *end) *s.bad = true;  // 1.5, true, 1e3
    else *s.v = (int32_t)(v < -1 ? -1 : v > 0x7FFFFFFF ? 0x7FFFFFFF : v);
    return;
  }
  if (strcmp(key_, "sound") == 0 || strcmp(key_, "wake") == 0) {
    bool sound = key_[0] == 's';
    (sound ? f_.has_sound : f_.has_wake) = true;
    if (is_true || is_false) (sound ? f_.sound : f_.wake) = is_true;
    else (sound ? f_.sound_bad : f_.wake_bad) = true;
  }
}

// Escalar conhecido mandado como string, objeto ou lista: tipo errado (400).
void NoticeJson::mark_bad() {
  if (strcmp(key_, "timeout") == 0) f_.timeout_bad = true;
  else if (strcmp(key_, "sound") == 0) f_.sound_bad = true;
  else if (strcmp(key_, "wake") == 0) f_.wake_bad = true;
  else if (strcmp(key_, "bar") == 0) f_.bar_bad = true;
  else if (strcmp(key_, "ttl") == 0) f_.ttl_bad = true;
}

void NoticeJson::step(char ch) {
  switch (state_) {
    case State::BeforeRoot:
      if (is_ws(ch)) return;
      state_ = ch == '{' ? State::Key : State::Error;
      return;

    case State::Key:
      if (is_ws(ch)) return;
      if (ch == '}') {
        state_ = State::Done;
        return;
      }
      if (ch != '"') {
        state_ = State::Error;
        return;
      }
      key_len_ = 0;
      key_[0] = '\0';
      string_is_key_ = true;
      escape_ = false;
      uni_left_ = 0;
      state_ = State::InString;
      return;

    case State::Colon:
      if (is_ws(ch)) return;
      state_ = ch == ':' ? State::Value : State::Error;
      return;

    case State::Value:
      if (is_ws(ch)) return;
      if (ch == '"') {
        target_ = nullptr;
        struct Slot {
          const char *key;
          char *buf;
          size_t cap;
        };
        const Slot slots[] = {
            {"id", f_.id, sizeof(f_.id)},
            {"tag", f_.tag, sizeof(f_.tag)},
            {"title", f_.title, sizeof(f_.title)},
            {"subtitle", f_.subtitle, sizeof(f_.subtitle)},
            {"message", f_.message, sizeof(f_.message)},
            {"type", f_.type, sizeof(f_.type)},
            {"priority", f_.priority, sizeof(f_.priority)},
            {"icon", f_.icon, sizeof(f_.icon)},
            {"color", f_.color, sizeof(f_.color)},
            {"text", f_.text, sizeof(f_.text)},
            {"text_color", f_.text_color, sizeof(f_.text_color)},
            {"actions", f_.actions, sizeof(f_.actions)},
        };
        for (const Slot &sl : slots) {
          if (strcmp(key_, sl.key) == 0) {
            target_ = sl.buf;
            target_cap_ = sl.cap;
            break;
          }
        }
        // Escalar mandado como string ("timeout":"10"): tipo errado.
        mark_bad();
        if (target_) {
          target_len_ = 0;
          target_[0] = '\0';
        }
        string_is_key_ = false;
        string_return_ = State::AfterValue;
        escape_ = false;
        uni_left_ = 0;
        hi_surrogate_ = 0;
        state_ = State::InString;
      } else if (ch == '{' || ch == '[') {
        mark_bad();
        in_actions_ = ch == '[' && strcmp(key_, "actions") == 0;
        if (in_actions_) f_.actions[0] = '\0';
        depth_ = 1;
        state_ = State::Nested;
      } else if (ch == '}' || ch == ',' || ch == ']' || ch == ':') {
        state_ = State::Error;
      } else {
        scalar_len_ = 0;
        scalar_[scalar_len_++] = ch;
        scalar_[scalar_len_] = '\0';
        state_ = State::Scalar;
      }
      return;

    case State::InString:
      if (uni_left_ > 0) {
        int v = (ch >= '0' && ch <= '9')   ? ch - '0'
                : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10
                : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10
                                           : -1;
        if (v < 0) {
          state_ = State::Error;
          return;
        }
        uni_ = (uni_ << 4) | (uint32_t)v;
        if (--uni_left_ == 0) {
          if (uni_ >= 0xD800 && uni_ < 0xDC00) {
            hi_surrogate_ = uni_;  // espera o par
          } else if (uni_ >= 0xDC00 && uni_ < 0xE000 && hi_surrogate_) {
            put_char(0x10000 + ((hi_surrogate_ - 0xD800) << 10) + (uni_ - 0xDC00));
            hi_surrogate_ = 0;
          } else {
            hi_surrogate_ = 0;
            put_char(uni_);
          }
        }
        return;
      }
      if (escape_) {
        escape_ = false;
        switch (ch) {
          case 'n': put_char('\n'); break;
          case 't': put_char('\t'); break;
          case 'r': put_char('\r'); break;
          case 'b': case 'f': break;
          case 'u':
            uni_left_ = 4;
            uni_ = 0;
            break;
          default: put_char((unsigned char)ch); break;  // \" \\ \/
        }
        return;
      }
      if (ch == '\\') {
        escape_ = true;
      } else if (ch == '"') {
        end_string();
      } else {
        put_byte(ch);
      }
      return;

    case State::Scalar:
      if (ch == ',' || ch == '}' || is_ws(ch)) {
        end_scalar();
        state_ = ch == ',' ? State::Key : ch == '}' ? State::Done : State::AfterValue;
      } else if (scalar_len_ + 1 < sizeof(scalar_)) {
        scalar_[scalar_len_++] = ch;
        scalar_[scalar_len_] = '\0';
      } else {
        scalar_len_ = sizeof(scalar_);  // comprido demais: não é número que sirva
      }
      return;

    case State::Nested:
      if (ch == '"') {
        target_ = nullptr;
        // Item da lista "actions": entra depois dos anteriores, com ','.
        if (in_actions_ && depth_ == 1) {
          size_t used = strlen(f_.actions);
          if (used && used + 2 < sizeof(f_.actions)) {
            f_.actions[used++] = ',';
            f_.actions[used] = '\0';
          }
          if (used + 1 < sizeof(f_.actions)) {
            target_ = f_.actions;
            target_cap_ = sizeof(f_.actions);
            target_len_ = used;
          }
        }
        string_is_key_ = false;
        string_return_ = State::Nested;
        escape_ = false;
        uni_left_ = 0;
        state_ = State::InString;
      } else if (ch == '{' || ch == '[') {
        depth_++;
      } else if (ch == '}' || ch == ']') {
        if (--depth_ == 0) {
          state_ = State::AfterValue;
          in_actions_ = false;
        }
      }
      return;

    case State::AfterValue:
      if (is_ws(ch)) return;
      if (ch == ',') state_ = State::Key;
      else if (ch == '}') state_ = State::Done;
      else state_ = State::Error;
      return;

    case State::Done:
    case State::Error:
      return;
  }
}
