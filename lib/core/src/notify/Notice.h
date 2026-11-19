#pragma once

#include <cstdint>

#include "notify/HookJson.h"

// Aviso do Claude Code (telas 15 e 15b). Só três tipos viram aviso; o resto
// dos eventos do hook é ignorado.
enum class NoticeKind : uint8_t { Permission, Idle, Done };

struct Notice {
  NoticeKind kind;
  char project[32];   // nome da pasta do projeto (fim do cwd), já cabendo na fonte
  char message[112];  // mensagem do Claude, já cabendo na fonte ("" = sem)
  char session[40];   // session_id do Claude Code ("" = desconhecida)
};

// Tipos como máscara (limpeza).
constexpr uint8_t notice_bit(NoticeKind k) { return (uint8_t)(1u << (uint8_t)k); }
constexpr uint8_t NOTICE_ALL = 0x07;

// "Você respondeu": tira os avisos de `kinds` da sessão `session`.
struct NoticeClear {
  char session[40];  // "" = qualquer sessão
  uint8_t kinds;
};
// O aviso `n` é apagado por `c`? Sessão vazia de um dos lados casa com tudo.
bool notice_matches(const Notice &n, const NoticeClear &c);

enum class HookAction : uint8_t { Ignore, Show, Clear };

// Campos do hook -> o que fazer.
//   Notification + permission_prompt -> Show Permission (message)
//   Notification + idle_prompt       -> Show Idle (message)
//   Stop                             -> Show Done (começo de last_assistant_message)
//   UserPromptSubmit, SessionEnd     -> Clear todos os tipos da sessão
//   PostToolBatch, PostToolUse,
//   PostToolUseFailure               -> Clear Permission da sessão (aprovou)
//   resto                            -> Ignore
HookAction hook_action(const HookFields &f, Notice *show, NoticeClear *clear);

// Mesmo chat do Claude: mesma sessão (session_id); sem sessão de um dos lados,
// mesmo projeto.
bool notice_same_chat(const Notice &a, const Notice &b);

// Os avisos abertos (faixa 15 / balões 15b). Um por chat (sessão): o novo
// substitui o anterior do mesmo chat (é o estado atual dele); dois Claude no
// mesmo projeto ficam com um aviso cada. Ordem: importância
// (Permission > Idle > Done, a ordem do enum) e, no empate, o mais antigo
// antes. at(0) é o que a faixa mostra.
constexpr uint8_t NOTICE_BOARD_CAP = 4;
constexpr uint32_t NOTICE_DEDUP_MS = 3000;

class NoticeBoard {
 public:
  // true = entrou (bipar). false = repetido (mesmo tipo e chat em menos de
  // NOTICE_DEDUP_MS) ou menos importante que todos com o quadro cheio.
  bool post(const Notice &n, uint32_t now_ms);
  // Tira os que `c` apaga. Retorna quantos saíram.
  uint8_t remove(const NoticeClear &c);
  // Fecha o de índice `i` (toque na faixa ou num balão).
  void dismiss(uint8_t i);
  uint8_t size() const { return count_; }
  const Notice &at(uint8_t i) const { return e_[i].n; }
  // Muda a cada alteração: a UI redesenha quando muda.
  uint32_t version() const { return version_; }

 private:
  struct Entry {
    Notice n;
    uint32_t seq;  // ordem de chegada
  };
  void sort();
  void erase(uint8_t i);

  Entry e_[NOTICE_BOARD_CAP];
  uint8_t count_ = 0;
  uint32_t seq_ = 0;
  uint32_t version_ = 0;
  // Último aceito (o repetido é ignorado mesmo depois de fechado).
  bool has_last_ = false;
  Notice last_{};
  uint32_t last_ms_ = 0;
};
