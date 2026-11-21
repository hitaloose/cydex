#pragma once

#include <cstdint>

#include "notify/NoticeJson.h"

// Aviso genérico (telas 15 e 15b), vindo do POST /notices. Quem manda decide
// título, texto, aparência e por quanto tempo fica; o Claude Code é só mais um
// cliente (tools/cydex_claude.sh).

enum class NoticeIcon : uint8_t { Info, Check, Alert, Shield, Clock, Bell };
// Ordem do quadro: High antes. A faixa mostra o primeiro.
enum class NoticePriority : uint8_t { High, Normal, Low };

constexpr uint32_t NOTICE_MAX_TIMEOUT_S = 86400;  // 24 h

struct Notice {
  char id[41];        // [A-Za-z0-9._:-], até 40. Mesmo id substitui o aberto
  char tag[17];       // rótulo pra fechar seletivo (DELETE ?tag=), "" = sem
  char title[48];     // já cabendo na fonte (text_fit)
  char subtitle[32];  // origem/contexto ("" = sem)
  char message[112];  // corpo ("" = sem)
  uint32_t color;     // 0xRRGGBB
  uint8_t type;       // preset (índice; nome em notice_type_name)
  NoticeIcon icon;
  NoticePriority priority;
  bool sound;         // toca o chime (se "Som ao tocar")
  bool wake;          // acende a tela
  uint32_t timeout_s; // 0 = até tocar ou DELETE
};

// Campo que fez o POST ser recusado (400). Vai no corpo da resposta.
enum class NoticeError : uint8_t {
  None, Title, Id, Tag, Type, Priority, Color, Timeout, Sound, Wake,
};
const char *notice_error_field(NoticeError e);

// Campos do JSON -> aviso. Presets do `type` (padrão "info") dão cor, ícone e
// prioridade; `color`, `icon` e `priority` sobrescrevem. Ícone desconhecido
// cai no do preset (ícones novos não quebram firmware antigo); os outros
// valores desconhecidos são erro.
NoticeError notice_from_fields(const NoticeFields &f, Notice *out);

// Nomes aceitos (também usados pelo GET /notices).
const char *notice_type_name(const Notice &n);
bool notice_valid_id(const char *s, unsigned max_len);

enum class PostResult : uint8_t {
  Added,     // entrou (bipar/acender conforme sound/wake)
  Repeated,  // mesmo id e título em menos de NOTICE_DEDUP_MS: ignorado
  Dropped,   // quadro cheio e menos importante que todos
};

// Os avisos abertos (faixa 15 / balões 15b). Um por id: o novo substitui o
// anterior do mesmo id (é o estado atual daquela coisa). Ordem: prioridade
// e, no empate, o mais antigo antes. at(0) é o que a faixa mostra.
constexpr uint8_t NOTICE_BOARD_CAP = 4;
constexpr uint32_t NOTICE_DEDUP_MS = 3000;

class NoticeBoard {
 public:
  // Sem id, gera um ("n<seq>"). Em todos os casos `n.id` sai com o id que
  // vale (no Repeated, o do aviso que já estava).
  PostResult post(Notice &n, uint32_t now_ms);
  // Fecha o `id` (com `tag` não vazia, só se a tag bater). Retorna quantos saíram.
  uint8_t remove(const char *id, const char *tag);
  uint8_t remove_all();
  // Tira os que passaram do timeout. Retorna quantos saíram.
  uint8_t expire(uint32_t now_ms);
  // Fecha o de índice `i` (toque na faixa ou num balão).
  void dismiss(uint8_t i);
  uint8_t size() const { return count_; }
  const Notice &at(uint8_t i) const { return e_[i].n; }
  uint32_t posted_ms(uint8_t i) const { return e_[i].posted_ms; }
  // Muda a cada alteração: a UI redesenha quando muda.
  uint32_t version() const { return version_; }

 private:
  struct Entry {
    Notice n;
    uint32_t seq;        // ordem de chegada
    uint32_t posted_ms;
  };
  void sort();
  void erase(uint8_t i);
  void changed();

  Entry e_[NOTICE_BOARD_CAP];
  uint8_t count_ = 0;
  uint32_t seq_ = 0;
  uint32_t auto_id_ = 0;
  uint32_t version_ = 0;
  // Último aceito (o repetido é ignorado mesmo depois de fechado).
  bool has_last_ = false;
  char last_given_id_[41] = "";  // o id que veio no POST ("" = gerado)
  char last_id_[41] = "";        // o id que ficou
  char last_title_[48] = "";
  uint32_t last_ms_ = 0;
};
