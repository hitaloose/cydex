#pragma once

#include <cstddef>
#include <cstdint>

// Leitor de JSON em fluxo para o corpo do POST /notices, byte a byte e com
// memória fixa. Guarda só as chaves de primeiro nível que interessam, cortadas
// no tamanho dos buffers. O resto (chaves desconhecidas, objetos, listas) é
// pulado sem guardar nada, então campos novos não quebram clientes antigos.
//
// Os buffers de texto são maiores que os do Notice: o text_fit() ainda tira
// markdown e junta espaços antes de cortar.
struct NoticeFields {
  char id[48];
  char tag[24];
  char title[96];
  char subtitle[64];
  char message[192];
  char type[16];
  char priority[16];
  char icon[16];
  char color[16];
  // Escalares. has_* = a chave veio; *_bad = veio com tipo/valor que não é o
  // esperado (vira 400).
  bool has_timeout, has_sound, has_wake;
  bool timeout_bad, sound_bad, wake_bad;
  int32_t timeout;  // segundos
  bool sound, wake;
};

class NoticeJson {
 public:
  NoticeJson() { reset(); }
  void reset();
  // Consome um pedaço do corpo. Pode ser chamado quantas vezes precisar.
  void feed(const char *data, size_t len);
  // O objeto de primeiro nível foi fechado sem erro.
  bool done() const { return state_ == State::Done; }
  bool failed() const { return state_ == State::Error; }
  const NoticeFields &fields() const { return f_; }

 private:
  enum class State : uint8_t {
    BeforeRoot,  // esperando '{'
    Key,         // esperando chave ou '}'
    Colon,
    Value,
    InString,    // valor string (de 1o nível ou aninhado)
    Scalar,      // número, true, false, null
    Nested,      // dentro de objeto/lista (só contando profundidade)
    AfterValue,  // esperando ',' ou '}'
    Done,
    Error,
  };

  void put_char(uint32_t cp);
  void put_byte(char b);
  void end_string();
  void end_scalar();
  void step(char ch);

  NoticeFields f_;
  State state_;
  State string_return_;   // estado depois da string atual
  bool string_is_key_;
  bool escape_;
  uint8_t uni_left_;      // dígitos hex restantes de \uXXXX
  uint32_t uni_;
  uint32_t hi_surrogate_;
  uint16_t depth_;        // profundidade dentro de Nested
  char key_[32];
  uint8_t key_len_;
  char *target_;          // buffer do valor atual (nullptr = descartar)
  size_t target_cap_;
  size_t target_len_;
  char scalar_[16];       // escalar de 1o nível (timeout, sound, wake)
  uint8_t scalar_len_;
};
