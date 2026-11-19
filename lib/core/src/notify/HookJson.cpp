#include "HookJson.h"

#include <cstring>

void HookJson::reset() {
  memset(&f_, 0, sizeof(f_));
  state_ = State::BeforeRoot;
  string_return_ = State::Key;
  string_is_key_ = false;
  escape_ = false;
  uni_left_ = 0;
  uni_ = 0;
  hi_surrogate_ = 0;
  depth_ = 0;
  key_len_ = 0;
  key_[0] = '\0';
  target_ = nullptr;
  target_cap_ = 0;
  target_len_ = 0;
  target_tail_ = false;
}

void HookJson::feed(const char *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (state_ == State::Done || state_ == State::Error) return;
    step(data[i]);
  }
}

static bool is_ws(char ch) { return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t'; }

// Escreve um código no buffer do valor (UTF-8). Começo: para quando enche,
// sem cortar caractere. Fim (cwd): desliza e mantém os últimos bytes.
void HookJson::put_char(uint32_t cp) {
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
  if (target_len_ + n < target_cap_) {
    memcpy(target_ + target_len_, buf, n);
    target_len_ += n;
    target_[target_len_] = '\0';
  } else if (target_tail_) {
    // Tira caracteres inteiros do começo até caber.
    size_t drop = 0;
    while (target_len_ - drop + n >= target_cap_) {
      drop++;
      while (drop < target_len_ && ((unsigned char)target_[drop] & 0xC0) == 0x80) drop++;
    }
    memmove(target_, target_ + drop, target_len_ - drop);
    target_len_ -= drop;
    memcpy(target_ + target_len_, buf, n);
    target_len_ += n;
    target_[target_len_] = '\0';
  }
}

void HookJson::end_string() {
  if (string_is_key_) {
    state_ = State::Colon;
    return;
  }
  target_ = nullptr;
  state_ = string_return_;
}

void HookJson::step(char ch) {
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
        target_tail_ = false;
        if (strcmp(key_, "hook_event_name") == 0) {
          target_ = f_.event;
          target_cap_ = sizeof(f_.event);
        } else if (strcmp(key_, "notification_type") == 0) {
          target_ = f_.type;
          target_cap_ = sizeof(f_.type);
        } else if (strcmp(key_, "message") == 0) {
          target_ = f_.message;
          target_cap_ = sizeof(f_.message);
        } else if (strcmp(key_, "last_assistant_message") == 0) {
          target_ = f_.last;
          target_cap_ = sizeof(f_.last);
        } else if (strcmp(key_, "session_id") == 0) {
          target_ = f_.session;
          target_cap_ = sizeof(f_.session);
        } else if (strcmp(key_, "cwd") == 0) {
          target_ = f_.cwd;
          target_cap_ = sizeof(f_.cwd);
          target_tail_ = true;
        }
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
        depth_ = 1;
        state_ = State::Nested;
      } else if (ch == '}' || ch == ',' || ch == ']' || ch == ':') {
        state_ = State::Error;
      } else {
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
      } else if (string_is_key_ || target_) {
        // Bytes UTF-8 crus passam como estão (put_char de 1 byte cada).
        unsigned char b = (unsigned char)ch;
        if (string_is_key_) {
          if (key_len_ + 1 < sizeof(key_)) {
            key_[key_len_++] = (char)b;
            key_[key_len_] = '\0';
          }
        } else if (target_len_ + 1 < target_cap_) {
          target_[target_len_++] = (char)b;
          target_[target_len_] = '\0';
        } else if (target_tail_) {
          memmove(target_, target_ + 1, target_len_ - 1);
          target_[target_len_ - 1] = (char)b;
        }
      }
      return;

    case State::Scalar:
      if (ch == ',') state_ = State::Key;
      else if (ch == '}') state_ = State::Done;
      else if (is_ws(ch)) state_ = State::AfterValue;
      return;

    case State::Nested:
      if (ch == '"') {
        target_ = nullptr;
        string_is_key_ = false;
        string_return_ = State::Nested;
        escape_ = false;
        uni_left_ = 0;
        state_ = State::InString;
      } else if (ch == '{' || ch == '[') {
        depth_++;
      } else if (ch == '}' || ch == ']') {
        if (--depth_ == 0) state_ = State::AfterValue;
      }
      return;

    case State::AfterValue:
      if (is_ws(ch)) return;
      if (ch == ',') state_ = State::Key;
      else if (ch == '}') state_ = State::Done;
      else state_ = State::Error;
      return;

    case State::InKey:
    case State::Done:
    case State::Error:
      return;
  }
}
