#pragma once

#include <cstdint>

#include "notify/Notice.h"
#include "notify/NoticeJson.h"

// Indicadores (PUT /status/<id>, tela 14b): estado contínuo de alguma coisa,
// mostrado como um chip pequeno no rodapé da Vitrine. O firmware não sabe o
// que eles são: o hook do Claude Code manda "claude" (sessões) e a statusline
// manda "quota" e "quota7d" (cota usada de 5 h e de 7 dias), mas qualquer cliente pode mandar outro.
//
// Corpo (o mesmo leitor do POST /notices):
//   text        obrigatório, curto ("2", "5h 20%"; cortado em 23 B)
//   icon        info, check, alert, shield, clock, bell, terminal (desconhecido = sem ícone)
//   color       #RRGGBB do ícone e da barra (padrão muted)
//   text_color  #RRGGBB do texto (padrão text)
//   bar         0-100: barra de 28x4 depois do texto (ausente = sem barra)
//   ttl         segundos até sumir sozinho (0 = até DELETE; padrão 0, máx 24 h)

constexpr uint8_t STATUS_BOARD_CAP = 4;  // claude + as 2 cotas + 1 livre
constexpr uint32_t STATUS_MAX_TTL_S = 86400;

struct StatusItem {
  char id[17];      // [A-Za-z0-9._:-], até 16
  char text[24];    // já cabendo na fonte (text_fit); ex.: "5h 20%"
  bool has_icon;
  NoticeIcon icon;
  uint32_t color;       // ícone e barra
  uint32_t text_color;
  int8_t bar;           // 0-100, -1 = sem barra
  uint32_t ttl_s;       // 0 = sem prazo
};

enum class StatusError : uint8_t { None, Id, Text, Color, TextColor, Bar, Ttl, Full };
const char *status_error_field(StatusError e);

StatusError status_from_fields(const NoticeFields &f, const char *id, StatusItem *out);

// Ordem de chegada (o primeiro que entrou fica à esquerda). PUT do mesmo id
// troca no lugar; se nada mudou, só renova o prazo e a versão não muda (o hook
// manda o mesmo estado a cada evento, e a Vitrine não deve redesenhar).
class StatusBoard {
 public:
  StatusError put(const StatusItem &it, uint32_t now_ms);
  uint8_t remove(const char *id);
  uint8_t remove_all();
  uint8_t expire(uint32_t now_ms);
  uint8_t size() const { return count_; }
  const StatusItem &at(uint8_t i) const { return e_[i].it; }
  uint32_t set_ms(uint8_t i) const { return e_[i].set_ms; }
  uint32_t version() const { return version_; }

 private:
  struct Entry {
    StatusItem it;
    uint32_t set_ms;
  };
  void erase(uint8_t i);

  Entry e_[STATUS_BOARD_CAP];
  uint8_t count_ = 0;
  uint32_t version_ = 0;
};
