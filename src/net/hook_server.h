#pragma once

#include "notify/Notice.h"

// Servidor HTTP do hook do Claude Code (porta 80, http://cydex.local/hook).
//
//   POST /hook  Authorization: Bearer <chave>, corpo = JSON do hook. Mostra
//               (Notification, Stop) ou limpa (UserPromptSubmit, PostToolBatch,
//               SessionEnd) os avisos da sessão.
//               204 sem corpo (o Claude Code lê corpo vazio como "seguir";
//               nunca bloqueia um Stop), 401 com chave errada, 400 JSON ruim,
//               413 com corpo > 16 KB (recusado sem ler).
//   GET  /      {"name":"CYDex","version":"0.1"} (teste com curl)
//
// Um cliente por vez, lido aos poucos a cada hook_server_tick() (no loop,
// mesma task do LVGL: sem mutex). O corpo passa pelo HookJson em fluxo, sem
// ficar na RAM. Quem sobe e derruba o servidor é o wifi_mgr.
void hook_server_start();
void hook_server_stop();
void hook_server_tick();

// Avisos abertos (um por chat, por importância). O servidor põe e limpa; o
// ui/notice_center desenha e fecha (toque).
NoticeBoard &hook_notices();
// Conta os avisos aceitos desde o boot (o notice_center bipa quando muda).
uint32_t hook_arrivals();
