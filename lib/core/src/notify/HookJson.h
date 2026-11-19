#pragma once

#include <cstddef>
#include <cstdint>

// Leitor de JSON em fluxo para o corpo do hook do Claude Code, byte a byte e
// com memória fixa. Guarda só as chaves de primeiro nível que interessam,
// cortadas no tamanho dos buffers. O resto (objetos, listas, números) é
// pulado sem guardar nada.
//
// O body do Stop traz `last_assistant_message`, que pode ter dezenas de KB.
// Ler o corpo inteiro na RAM não dá no CYD (sem PSRAM).
struct HookFields {
  char event[24];          // hook_event_name
  char type[32];           // notification_type
  char message[160];       // message (Notification)
  char last[160];          // last_assistant_message (Stop), só o começo
  char cwd[96];            // cwd: só o fim importa (nome da pasta), ver abaixo
  char session[40];        // session_id (UUID de 36): liga o aviso ao que o limpa
};

class HookJson {
 public:
  HookJson() { reset(); }
  void reset();
  // Consome um pedaço do corpo. Pode ser chamado quantas vezes precisar.
  void feed(const char *data, size_t len);
  // O objeto de primeiro nível foi fechado sem erro.
  bool done() const { return state_ == State::Done; }
  bool failed() const { return state_ == State::Error; }
  const HookFields &fields() const { return f_; }

 private:
  enum class State : uint8_t {
    BeforeRoot,  // esperando '{'
    Key,         // esperando chave ou '}'
    InKey,
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
  void end_string();
  void step(char ch);

  HookFields f_;
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
  bool target_tail_;      // guarda o FIM (cwd), não o começo
};
