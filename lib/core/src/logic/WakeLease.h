#pragma once

#include <cstdint>

// Rede de segurança do POST /wake?ttl=<s> (api 5). O app do computador
// renova a cada status (30 s) enquanto a tela dele está acesa. Se ninguém
// renovar dentro do prazo, o computador sumiu sem avisar (dormiu sem o
// /sleep sair, Wi-Fi caiu, app fechou): o CYD entra no mesmo modo do /sleep.
//
// - Com prazo valendo, o /wake?ttl só estende (não acende nem conta como
//   toque: o "apagar tela" por inatividade continua valendo).
// - Sem prazo valendo, funciona como o /wake e começa o prazo.
// - /sleep, /wake sem ttl e toque na tela cancelam o prazo.
//
// Os tempos são do millis() (32 bits, conta com a volta).
class WakeLease {
 public:
  static constexpr uint32_t kMaxTtl = 3600;  // s

  // Renova (ou começa) o prazo. Devolve true se não havia prazo valendo:
  // quem chama trata como um /wake de verdade.
  bool renew(uint32_t now_ms, uint32_t ttl_s);
  void clear() { active_ = false; }
  bool active() const { return active_; }
  // true uma vez, quando o prazo acaba (e já cancela).
  bool expired(uint32_t now_ms);
  // ms até acabar (0 sem prazo).
  uint32_t left_ms(uint32_t now_ms) const;

 private:
  bool active_ = false;
  uint32_t start_ = 0;
  uint32_t ttl_ms_ = 0;
};

// Lê `ttl` da query do /wake. Sem `ttl` (ou ttl=0): *ttl_s = 0 e devolve
// nullptr. Inválido (não é inteiro, passa de kMaxTtl): devolve "ttl", o campo
// do 400.
const char *wake_lease_parse(const char *query, uint32_t *ttl_s);
