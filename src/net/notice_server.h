#pragma once

#include "notify/Notice.h"
#include "notify/Status.h"

// Servidor HTTP da API (porta 80, http://cydex.local): avisos, saves e Dex. Qualquer coisa na
// rede manda aviso; o Claude Code é só um cliente (tools/cydex_claude.sh).
// Todas as rotas, menos GET /, pedem Authorization: Bearer <chave>.
//
//   POST   /notices         corpo JSON (title, subtitle, message, id, tag, type,
//                           priority, icon, color, timeout, sound, wake, actions).
//                           201 {"id":"...","rev":N}; 400 {"error":"<campo>"} ou
//                           {"error":"json"}; 413 com corpo > 4 KB (sem ler).
//   GET    /notices/<id>    {"id","rev","answer"}: answer null = aberto, "allow"/"deny"
//                           = respondido na tela (guardado 2 min). 404 = fechado.
//   PUT    /status/<id>     indicador do rodapé da Vitrine (notify/Status.h). 204;
//                           400 {"error":"<campo>"}; 409 {"error":"full"}.
//   DELETE /status/<id>     204. DELETE /status fecha todos. GET /status lista.
//   DELETE /notices/<id>    fecha o aviso (?tag=x: só se a tag bater). 204.
//   DELETE /notices         fecha todos. 204.
//   GET    /notices         [{"id","tag","title","type","age"}] (depuração).
//   POST   /sleep, /wake    sem corpo: apaga a tela (até na Vitrine) ou acende.
//                           O app do computador (desktop/) manda quando ele dorme/acorda. 204.
//   POST   /wake?ttl=<s>    api 5, 1–3600 s: rede de segurança (logic/WakeLease).
//                           Sem renovar no prazo, entra no modo do /sleep. 400 "ttl".
//   GET    /saves           [{"name","size","mtime","format"}] (format null =
//                           não reconhecido). Até 16.
//   PUT    /saves/<nome>    corpo = o save, gravado em fluxo num temporário e
//                           renomeado no fim. 201; 400 {"error":"name"};
//                           411 sem Content-Length; 413 > 1 MB; 507 erro de escrita.
//   DELETE /saves/<nome>    204 (idempotente).
//   POST   /saves/reload    refaz o status dos saves (ignora o cache) e remonta a
//                           tela. 200 {"saves","parsed","seen","caught"}.
//   GET    /dex             {"count":1025,"seen":"<hex>","caught":"<hex>"} (bit = n-1).
//   GET    /                {"name":"CYDex","version":"0.1","api":4,"rssi"} (teste com curl)
//   401 chave errada (sem ler o corpo), 404, 405, 503 sem cartão.
//
// Um cliente por vez, lido aos poucos a cada notice_server_tick() (no loop,
// mesma task do LVGL: sem mutex). O corpo passa pelo NoticeJson em fluxo, sem
// ficar na RAM. Quem sobe e derruba o servidor é o wifi_mgr.
void notice_server_start();
void notice_server_stop();
void notice_server_tick();

// Avisos abertos (por prioridade). O servidor põe e tira; o ui/notice_center
// desenha, expira e fecha (toque).
NoticeBoard &notices();
// Indicadores (PUT /status). O servidor põe e tira; a Vitrine desenha e o
// ui/notice_center expira.
StatusBoard &statuses();

// O que os avisos que entraram desde a última chamada pedem (e zera).
constexpr uint8_t NOTICE_ARRIVAL_WAKE = 1;
constexpr uint8_t NOTICE_ARRIVAL_SOUND = 2;
uint8_t notice_take_arrivals();
