#pragma once

#include <cstdint>

// Brilho e "apagar tela" (Ajustes 05). Telas pedem o brilho por aqui, não
// direto no backlight, pra que acordar volte ao nível certo.
void power_init();
// Chamar no loop: apaga a tela depois do tempo sem toque (nunca na Vitrine).
void power_tick();
// Nível fixo (%), fora do dia/noite: só a prévia dos sliders de Ajustes.
void power_set_level(uint8_t percent);
// Brilho normal dos Ajustes (o de dia ou o de noite, conforme o sol).
void power_use_normal_level();
// Brilho da Vitrine (dia ou noite).
void power_use_showcase_level();
// Já passou do pôr do sol? false sem localidade ou sem hora (NTP).
bool power_is_night();
// A localidade mudou (PUT/DELETE /location): reavalia dia/noite na hora.
void power_location_changed();
// Acende a tela (se apagada) e zera o tempo sem toque. Avisos do Claude Code.
void power_wake();
// O computador dormiu (POST /sleep): apaga a tela, até na Vitrine, mas não
// enquanto houver aviso aberto. Aviso novo acende só o CYD e, quando o último
// fecha, apaga de novo. Toque (usar o CYD) ou POST /wake saem desse modo.
void power_remote_sleep();
void power_remote_wake();
