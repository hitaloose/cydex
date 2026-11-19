#include "wifi_mgr.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include <cstring>

#include "data/Settings.h"
#include "net/hook_server.h"
#include "util/log.h"

static const char *TAG = "wifi";
static const char *kHostname = "cydex";
static const uint32_t kConnectTimeoutMs = 15000;
static const uint32_t kRetryMinMs = 5000;
static const uint32_t kRetryMaxMs = 60000;

static WifiState s_state = WifiState::Off;
static WifiFail s_fail = WifiFail::None;
static uint32_t s_version = 1;

static char s_ssid[WIFI_SSID_MAX + 1];   // tentando ou conectado
static char s_pass[64];
static bool s_user_attempt = false;       // veio da tela: salvar se der certo
static uint32_t s_attempt_at = 0;
static uint32_t s_retry_at = 0;           // reconexão da rede salva (0 = nenhuma)
static uint32_t s_retry_ms = kRetryMinMs;
static char s_ip[16];
static bool s_mdns = false;

static volatile uint8_t s_last_reason = 0;  // escrito pelo evento do Wi-Fi
static volatile bool s_got_ip = false;
static volatile bool s_lost = false;

static WifiList s_list;
static bool s_scanning = false;
static char s_pending[WIFI_SSID_MAX + 1];

static void bump() { s_version++; }

static void set_state(WifiState st) {
  if (s_state == st) return;
  s_state = st;
  bump();
}

// Os eventos vêm da task do Wi-Fi: só marcam flags, o resto é no wifi_tick.
static void on_event(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      s_got_ip = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      s_last_reason = info.wifi_sta_disconnected.reason;
      s_lost = true;
      break;
    default:
      break;
  }
}

static WifiFail reason_to_fail(uint8_t r) {
  switch (r) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_MIC_FAILURE:
      return WifiFail::WrongPassword;
    case WIFI_REASON_NO_AP_FOUND:
      return WifiFail::NotFound;
    default:
      return WifiFail::Other;
  }
}

static void radio_on() {
  if (WiFi.getMode() == WIFI_STA) return;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.setAutoReconnect(false);  // a reconexão é nossa (com espera crescente)
  // Sem modem sleep: com ele o ping ia a 300-850 ms e um pacote perdido
  // segurava o POST do hook por 3-5 s (o hook tem 3 s). O CYD fica na USB.
  WiFi.setSleep(false);
}

static void begin_attempt(const char *ssid, const char *pass, bool user) {
  radio_on();
  strncpy(s_ssid, ssid, WIFI_SSID_MAX);
  s_ssid[WIFI_SSID_MAX] = '\0';
  strncpy(s_pass, pass ? pass : "", sizeof(s_pass) - 1);
  s_pass[sizeof(s_pass) - 1] = '\0';
  s_user_attempt = user;
  s_fail = WifiFail::None;
  s_got_ip = false;
  s_lost = false;
  s_last_reason = 0;
  s_retry_at = 0;
  WiFi.disconnect(false, false);
  WiFi.begin(s_ssid, s_pass[0] ? s_pass : nullptr);
  s_attempt_at = millis();
  set_state(WifiState::Connecting);
  bump();
  LOG_I(TAG, "conectando em \"%s\"%s", s_ssid, user ? " (pela tela)" : "");
}

static void stop_services() {
  hook_server_stop();
  if (s_mdns) {
    MDNS.end();
    s_mdns = false;
  }
  s_ip[0] = '\0';
}

static void on_connected() {
  IPAddress ip = WiFi.localIP();
  snprintf(s_ip, sizeof(s_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  s_retry_ms = kRetryMinMs;
  s_retry_at = 0;
  if (s_user_attempt) {
    AppSettings &st = settings();
    strncpy(st.wifi_ssid, s_ssid, sizeof(st.wifi_ssid) - 1);
    st.wifi_ssid[sizeof(st.wifi_ssid) - 1] = '\0';
    strncpy(st.wifi_pass, s_pass, sizeof(st.wifi_pass) - 1);
    st.wifi_pass[sizeof(st.wifi_pass) - 1] = '\0';
    settings_save_wifi();
    s_user_attempt = false;
  }
  if (MDNS.begin(kHostname)) {
    MDNS.addService("http", "tcp", 80);
    s_mdns = true;
  } else {
    LOG_E(TAG, "mDNS não subiu");
  }
  hook_server_start();
  set_state(WifiState::Connected);
  LOG_I(TAG, "conectado em \"%s\", ip=%s (http://%s.local) heap=%u maior_bloco=%u", s_ssid, s_ip,
        kHostname, ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

// Rede salva: agenda outra tentativa (5 s, 10 s, 20 s... até 60 s).
static void schedule_retry() {
  const AppSettings &st = settings();
  if (!st.wifi_on || !st.wifi_ssid[0]) return;
  s_retry_at = millis() + s_retry_ms;
  if (s_retry_at == 0) s_retry_at = 1;
  LOG_I(TAG, "nova tentativa em %u s", (unsigned)(s_retry_ms / 1000));
  s_retry_ms = s_retry_ms * 2 > kRetryMaxMs ? kRetryMaxMs : s_retry_ms * 2;
}

static void on_failed(WifiFail why) {
  WiFi.disconnect(false, false);
  stop_services();
  s_fail = why;
  set_state(WifiState::Failed);
  bump();
  LOG_I(TAG, "falhou em \"%s\" (motivo %u)", s_ssid, (unsigned)s_last_reason);
  bool was_user = s_user_attempt;
  s_user_attempt = false;
  // Tentativa da tela que falhou: volta a tentar a rede salva (se outra).
  const AppSettings &st = settings();
  if (was_user && st.wifi_ssid[0] && strcmp(st.wifi_ssid, s_ssid) != 0) {
    s_retry_ms = kRetryMinMs;
  }
  schedule_retry();
}

void wifi_mgr_init() {
  WiFi.onEvent(on_event);
  const AppSettings &st = settings();
  if (!st.wifi_on) {
    WiFi.mode(WIFI_OFF);
    s_state = WifiState::Off;
    return;
  }
  if (st.wifi_ssid[0]) begin_attempt(st.wifi_ssid, st.wifi_pass, false);
  else {
    radio_on();
    s_state = WifiState::Idle;
  }
}

void wifi_tick() {
  hook_server_tick();
  if (s_state == WifiState::Off) return;

  if (s_scanning) {
    int16_t n = WiFi.scanComplete();
    if (n != WIFI_SCAN_RUNNING) {
      s_list.clear();
      for (int16_t i = 0; i < n; i++) {
        s_list.add(WiFi.SSID(i).c_str(), (int8_t)WiFi.RSSI(i),
                   WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      }
      WiFi.scanDelete();
      s_list.sort(s_state == WifiState::Connected ? s_ssid : nullptr);
      s_scanning = false;
      bump();
      LOG_I(TAG, "busca: %d redes (%u na lista)", n < 0 ? 0 : n, s_list.count);
    }
  }

  switch (s_state) {
    case WifiState::Connecting:
      if (s_got_ip) {
        s_got_ip = false;
        s_lost = false;
        on_connected();
      } else if (s_lost && reason_to_fail(s_last_reason) != WifiFail::Other) {
        // Senha errada ou rede sumida: não adianta esperar o timeout.
        on_failed(reason_to_fail(s_last_reason));
      } else if (millis() - s_attempt_at > kConnectTimeoutMs) {
        on_failed(s_last_reason ? reason_to_fail(s_last_reason) : WifiFail::Other);
      }
      break;
    case WifiState::Connected:
      if (s_lost) {
        s_lost = false;
        LOG_I(TAG, "caiu (motivo %u)", (unsigned)s_last_reason);
        stop_services();
        set_state(WifiState::Idle);
        s_retry_ms = kRetryMinMs;
        schedule_retry();
      }
      break;
    case WifiState::Idle:
    case WifiState::Failed:
      if (s_retry_at && (int32_t)(millis() - s_retry_at) >= 0 && !s_scanning) {
        s_retry_at = 0;
        const AppSettings &st = settings();
        if (st.wifi_on && st.wifi_ssid[0]) begin_attempt(st.wifi_ssid, st.wifi_pass, false);
      }
      break;
    case WifiState::Off:
      break;
  }
}

void wifi_set_enabled(bool on) {
  AppSettings &st = settings();
  if (st.wifi_on == on) return;
  st.wifi_on = on;
  settings_save_wifi();
  if (on) {
    s_retry_ms = kRetryMinMs;
    if (st.wifi_ssid[0]) begin_attempt(st.wifi_ssid, st.wifi_pass, false);
    else {
      radio_on();
      set_state(WifiState::Idle);
    }
  } else {
    stop_services();
    s_scanning = false;
    s_retry_at = 0;
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    s_list.clear();
    set_state(WifiState::Off);
  }
  bump();
}

WifiState wifi_state() { return s_state; }
WifiFail wifi_fail_reason() { return s_fail; }
const char *wifi_ssid() { return s_state == WifiState::Off ? "" : s_ssid; }
const char *wifi_ip() { return s_state == WifiState::Connected ? s_ip : ""; }

void wifi_connect(const char *ssid, const char *pass) {
  if (s_state == WifiState::Off || !ssid || !ssid[0]) return;
  stop_services();
  s_scanning = false;
  begin_attempt(ssid, pass, true);
}

void wifi_forget() {
  AppSettings &st = settings();
  st.wifi_ssid[0] = '\0';
  st.wifi_pass[0] = '\0';
  settings_save_wifi();
  stop_services();
  s_retry_at = 0;
  s_ssid[0] = '\0';
  if (s_state != WifiState::Off) {
    WiFi.disconnect(false, false);
    set_state(WifiState::Idle);
  }
  s_list.sort(nullptr);
  bump();
}

void wifi_scan_start() {
  if (s_state == WifiState::Off || s_state == WifiState::Connecting || s_scanning) return;
  radio_on();
  int16_t r = WiFi.scanNetworks(true, false);
  if (r == WIFI_SCAN_FAILED) {
    LOG_E(TAG, "busca não começou");
    return;
  }
  s_scanning = true;
  bump();
}

bool wifi_scanning() { return s_scanning; }
const WifiList &wifi_scan_results() { return s_list; }
uint32_t wifi_version() { return s_version; }

void wifi_set_pending_ssid(const char *ssid) {
  strncpy(s_pending, ssid ? ssid : "", WIFI_SSID_MAX);
  s_pending[WIFI_SSID_MAX] = '\0';
}
const char *wifi_pending_ssid() { return s_pending; }
