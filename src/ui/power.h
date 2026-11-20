#pragma once

#include <cstdint>

// Brilho e "apagar tela" (Ajustes 05). Telas pedem o brilho por aqui, não
// direto no backlight, pra que acordar volte ao nível certo.
void power_init();
// Chamar no loop: apaga a tela depois do tempo sem toque (nunca na Vitrine).
void power_tick();
// Nível atual (%): o normal dos Ajustes ou o da Vitrine.
void power_set_level(uint8_t percent);
// Volta ao brilho normal dos Ajustes.
void power_use_normal_level();
// Acende a tela (se apagada) e zera o tempo sem toque. Avisos do Claude Code.
void power_wake();
// O computador dormiu (POST /sleep): apaga a tela, até na Vitrine, mas não
// enquanto houver aviso aberto. Aviso novo acende só o CYD e, quando o último
// fecha, apaga de novo. Toque (usar o CYD) ou POST /wake saem desse modo.
void power_remote_sleep();
void power_remote_wake();
