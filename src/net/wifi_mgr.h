#pragma once

#include <cstdint>

#include "net/WifiList.h"

// Wi-Fi do CYDex (telas 05d e 05e). Tudo avança em wifi_tick(), no loop, sem
// bloquear o LVGL. A rede e a senha ficam em AppSettings (NVS); o Wi-Fi do
// core não grava nada (WiFi.persistent(false)).
//
// Conectado: sobe o mDNS (cydex.local) e o servidor de avisos (net/notice_server).

enum class WifiState : uint8_t {
  Off,           // rádio desligado (Ajustes)
  Idle,          // ligado, sem rede salva ou depois de "Esquecer"
  Connecting,
  Connected,
  Failed,        // a última tentativa falhou (ver wifi_fail_reason)
};

enum class WifiFail : uint8_t { None, WrongPassword, NotFound, Other };

// Lê os Ajustes; se o Wi-Fi estiver ligado e houver rede salva, começa a
// conectar. Chamar depois de settings_init().
void wifi_mgr_init();
void wifi_tick();

// Ligar/desligar (interruptor do 05d). Salva nos Ajustes.
void wifi_set_enabled(bool on);
WifiState wifi_state();
WifiFail wifi_fail_reason();
// Rede em uso ou tentando ("" se nenhuma).
const char *wifi_ssid();
// "192.168.0.12" quando conectado, "" fora disso.
const char *wifi_ip();

// Conecta numa rede escolhida na tela. Só grava rede e senha nos Ajustes se
// der certo (senha errada não apaga a rede que já funcionava).
void wifi_connect(const char *ssid, const char *pass);
// Desconecta e apaga a rede salva.
void wifi_forget();

// Busca assíncrona (~2 s). Ignorada enquanto conecta ou com o rádio desligado.
void wifi_scan_start();
bool wifi_scanning();
// Resultado da última busca (a rede atual no topo).
const WifiList &wifi_scan_results();

// Muda a cada transição de estado ou fim de busca: as telas comparam com o
// último valor visto pra saber se precisam redesenhar.
uint32_t wifi_version();

// Rede escolhida na lista que a tela da Senha (05e) vai usar.
void wifi_set_pending_ssid(const char *ssid);
const char *wifi_pending_ssid();
