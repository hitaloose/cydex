#include "HttpRequest.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

void HttpRequest::reset() {
  state_ = State::RequestLine;
  line_len_ = 0;
  line_overflow_ = false;
  line_[0] = '\0';
  method_[0] = '\0';
  path_[0] = '\0';
  bearer_[0] = '\0';
  content_length_ = -1;
  body_left_ = 0;
}

static bool header_is(const char *line, const char *name, const char **value) {
  size_t n = strlen(name);
  for (size_t i = 0; i < n; i++) {
    if (!line[i] || tolower((unsigned char)line[i]) != name[i]) return false;
  }
  if (line[n] != ':') return false;
  const char *v = line + n + 1;
  while (*v == ' ' || *v == '\t') v++;
  *value = v;
  return true;
}

void HttpRequest::line_done() {
  line_[line_len_] = '\0';
  // Tira espaços do fim (e o \r que sobrou).
  while (line_len_ > 0 && (line_[line_len_ - 1] == ' ' || line_[line_len_ - 1] == '\r'))
    line_[--line_len_] = '\0';

  if (state_ == State::RequestLine) {
    if (line_overflow_) {
      state_ = State::Error;
      return;
    }
    // "POST /hook HTTP/1.1"
    const char *sp1 = strchr(line_, ' ');
    if (!sp1 || sp1 == line_ || (size_t)(sp1 - line_) >= sizeof(method_)) {
      state_ = State::Error;
      return;
    }
    memcpy(method_, line_, sp1 - line_);
    method_[sp1 - line_] = '\0';
    const char *p = sp1 + 1;
    const char *sp2 = strchr(p, ' ');
    size_t plen = sp2 ? (size_t)(sp2 - p) : strlen(p);
    if (plen == 0) {
      state_ = State::Error;
      return;
    }
    if (plen >= sizeof(path_)) plen = sizeof(path_) - 1;
    memcpy(path_, p, plen);
    path_[plen] = '\0';
    // Query string não interessa.
    char *q = strchr(path_, '?');
    if (q) *q = '\0';
    state_ = State::Headers;
  } else if (state_ == State::Headers) {
    if (line_len_ == 0 && !line_overflow_) {
      // Fim dos cabeçalhos.
      if (content_length_ > 0) {
        body_left_ = content_length_;
        state_ = State::Body;
      } else {
        state_ = State::Done;
      }
    } else if (!line_overflow_) {
      const char *v;
      if (header_is(line_, "content-length", &v)) {
        char *end;
        long n = strtol(v, &end, 10);
        if (end == v || n < 0) {
          state_ = State::Error;
          return;
        }
        content_length_ = (int32_t)(n > 0x7FFFFFFF ? 0x7FFFFFFF : n);
      } else if (header_is(line_, "authorization", &v)) {
        const char *prefix = "bearer ";
        bool bearer = true;
        for (int i = 0; i < 7; i++) {
          if (tolower((unsigned char)v[i]) != prefix[i]) {
            bearer = false;
            break;
          }
        }
        if (bearer) {
          v += 7;
          while (*v == ' ') v++;
          size_t n = strlen(v);
          if (n >= sizeof(bearer_)) n = sizeof(bearer_) - 1;
          memcpy(bearer_, v, n);
          bearer_[n] = '\0';
        }
      }
    }
  }
  line_len_ = 0;
  line_overflow_ = false;
}

size_t HttpRequest::feed(const char *data, size_t len, BodySink on_body, void *ctx) {
  size_t i = 0;
  while (i < len) {
    if (state_ == State::Done || state_ == State::Error) return i;
    if (state_ == State::Body) {
      size_t n = len - i;
      if ((int32_t)n > body_left_) n = (size_t)body_left_;
      if (on_body && n) on_body(ctx, data + i, n);
      body_left_ -= (int32_t)n;
      i += n;
      if (body_left_ == 0) state_ = State::Done;
      continue;
    }
    char ch = data[i++];
    if (ch == '\n') {
      line_done();
      continue;
    }
    // Cabeçalho comprido demais (cookies...) é ignorado, não quebra o pedido.
    if (line_len_ + 1 < sizeof(line_)) line_[line_len_++] = ch;
    else line_overflow_ = true;
  }
  return i;
}
