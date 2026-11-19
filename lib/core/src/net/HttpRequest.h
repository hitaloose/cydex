#pragma once

#include <cstddef>
#include <cstdint>

// Leitor de pedido HTTP/1.x em fluxo (servidor do hook, net/hook_server).
// Memória fixa: guarda método, caminho, Content-Length e Authorization; o
// corpo é entregue em pedaços pra quem chamou (HookJson), sem ficar na RAM.
class HttpRequest {
 public:
  enum class State : uint8_t { RequestLine, Headers, Body, Done, Error };

  HttpRequest() { reset(); }
  void reset();

  // Consome bytes. Os do corpo vão pra on_body (até Content-Length).
  // Retorna quantos bytes de `data` foram usados (para em Done/Error).
  typedef void (*BodySink)(void *ctx, const char *data, size_t len);
  size_t feed(const char *data, size_t len, BodySink on_body, void *ctx);

  State state() const { return state_; }
  bool done() const { return state_ == State::Done; }
  bool failed() const { return state_ == State::Error; }

  const char *method() const { return method_; }
  const char *path() const { return path_; }
  // -1 = sem Content-Length.
  int32_t content_length() const { return content_length_; }
  // Valor do Authorization (sem o "Bearer "), "" se ausente.
  const char *bearer() const { return bearer_; }

 private:
  void line_done();

  State state_;
  char line_[160];
  uint8_t line_len_;
  bool line_overflow_;
  char method_[8];
  char path_[48];
  char bearer_[48];
  int32_t content_length_;
  int32_t body_left_;
};
