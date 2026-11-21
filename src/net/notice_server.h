#pragma once

#include "notify/Notice.h"

// Servidor HTTP de avisos (porta 80, http://cydex.local). Qualquer coisa na
// rede manda aviso; o Claude Code é só um cliente (tools/cydex_claude.sh).
// Todas as rotas, menos GET /, pedem Authorization: Bearer <chave>.
//
//   POST   /notices         corpo JSON (title, subtitle, message, id, tag, type,
//                           priority, icon, color, timeout, sound, wake).
//                           201 {"id":"..."}; 400 {"error":"<campo>"} ou
//                           {"error":"json"}; 413 com corpo > 4 KB (sem ler).
//   DELETE /notices/<id>    fecha o aviso (?tag=x: só se a tag bater). 204.
//   DELETE /notices         fecha todos. 204.
//   GET    /notices         [{"id","tag","title","type","age"}] (depuração).
//   POST   /sleep, /wake    sem corpo: apaga a tela (até na Vitrine) ou acende.
//                           O Mac manda quando dorme/acorda (tools/cydex_power.sh). 204.
//   GET    /                {"name":"CYDex","version":"0.1"} (teste com curl)
//   401 chave errada (sem ler o corpo), 404, 405.
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

// O que os avisos que entraram desde a última chamada pedem (e zera).
constexpr uint8_t NOTICE_ARRIVAL_WAKE = 1;
constexpr uint8_t NOTICE_ARRIVAL_SOUND = 2;
uint8_t notice_take_arrivals();
