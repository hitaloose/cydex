#include "hook_server.h"

#include <WiFi.h>

#include <cstring>

#include "data/Settings.h"
#include "net/HttpRequest.h"
#include "notify/HookJson.h"
#include "ui/power.h"
#include "util/log.h"
#include "version.h"

static const char *TAG = "hook";
// 3 s sem chegar nada: cliente parado, fecha (senão segura o servidor).
static const uint32_t kClientTimeoutMs = 3000;
// Corpo maior que isso: 413 sem ler. Um body de 60 KB chegando mais rápido que
// o loop lê enchia os buffers do TCP, o maior bloco do heap ia a 1,7 KB e o
// Wi-Fi perdia pacotes. O hook do exemplo manda < 1 KB (filtra com jq).
static const int32_t kMaxBody = 16 * 1024;

static WiFiServer *s_server = nullptr;
static WiFiClient s_client;
static bool s_busy = false;
static uint32_t s_since = 0;
static HttpRequest s_req;
static HookJson s_json;
static NoticeBoard s_board;
static uint32_t s_arrivals = 0;

NoticeBoard &hook_notices() { return s_board; }
uint32_t hook_arrivals() { return s_arrivals; }

void hook_server_start() {
  if (s_server) return;
  // new uma vez: o WiFiServer fica vivo enquanto conectado (~100 B).
  s_server = new WiFiServer(80);
  s_server->begin();
  s_server->setNoDelay(true);
  LOG_I(TAG, "ouvindo na porta 80");
}

void hook_server_stop() {
  if (!s_server) return;
  if (s_busy) s_client.stop();
  s_busy = false;
  s_server->end();
  delete s_server;
  s_server = nullptr;
}

static void reply(int code, const char *reason, const char *body = nullptr) {
  char head[160];
  size_t blen = body ? strlen(body) : 0;
  int n = snprintf(head, sizeof(head),
                   "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Length: %u\r\n%s\r\n", code,
                   reason, (unsigned)blen, body ? "Content-Type: application/json\r\n" : "");
  s_client.write((const uint8_t *)head, n);
  if (blen) s_client.write((const uint8_t *)body, blen);
  s_client.flush();
  s_client.stop();
  s_busy = false;
}

// Caminhos que pedem a chave: o hook e o apagar/acender junto com o computador.
static bool needs_key(const char *path) {
  return strcmp(path, "/hook") == 0 || strcmp(path, "/sleep") == 0 || strcmp(path, "/wake") == 0;
}

static void on_body(void *, const char *data, size_t len) { s_json.feed(data, len); }

static void finish() {
  const char *method = s_req.method();
  const char *path = s_req.path();

  if (strcmp(path, "/") == 0 && strcmp(method, "GET") == 0) {
    reply(200, "OK", "{\"name\":\"CYDex\",\"version\":\"" CYDEX_VERSION "\"}");
    return;
  }
  if (!needs_key(path)) {
    reply(404, "Not Found");
    return;
  }
  if (strcmp(method, "POST") != 0) {
    reply(405, "Method Not Allowed");
    return;
  }
  if (strcmp(s_req.bearer(), settings().hook_key) != 0) {
    LOG_I(TAG, "chave errada de %s", s_client.remoteIP().toString().c_str());
    reply(401, "Unauthorized");
    return;
  }
  if (strcmp(path, "/sleep") == 0 || strcmp(path, "/wake") == 0) {
    bool sleep = path[1] == 's';
    LOG_I(TAG, "computador %s", sleep ? "dormiu" : "acordou");
    if (sleep) power_remote_sleep();
    else power_remote_wake();
    reply(204, "No Content");
    return;
  }
  if (!s_json.done()) {
    reply(400, "Bad Request");
    return;
  }
  Notice n;
  NoticeClear c;
  switch (hook_action(s_json.fields(), &n, &c)) {
    case HookAction::Show: {
      bool queued = s_board.post(n, millis());
      if (queued) s_arrivals++;
      LOG_I(TAG, "%s %s de \"%s\"%s", s_json.fields().event, s_json.fields().type, n.project,
            queued ? "" : " (repetido)");
      break;
    }
    case HookAction::Clear: {
      uint8_t removed = s_board.remove(c);
      LOG_I(TAG, "%s: limpa sessão %.8s (%u avisos)", s_json.fields().event, c.session, removed);
      break;
    }
    case HookAction::Ignore:
      LOG_D(TAG, "ignorado: %s %s", s_json.fields().event, s_json.fields().type);
      break;
  }
  reply(204, "No Content");
}

void hook_server_tick() {
  if (!s_server) return;
  if (!s_busy) {
    s_client = s_server->available();
    if (!s_client) return;
    s_busy = true;
    s_since = millis();
    s_req.reset();
    s_json.reset();
  }
  if (!s_client.connected() && !s_client.available()) {
    s_client.stop();
    s_busy = false;
    return;
  }
  // Lê o que já chegou, em pedaços de 256 B (pilha), e devolve o loop.
  char buf[256];
  int budget = 32;  // até 8 KB por volta do loop
  while (s_client.available() && budget-- > 0 && !s_req.done() && !s_req.failed()) {
    int n = s_client.read((uint8_t *)buf, sizeof(buf));
    if (n <= 0) break;
    s_since = millis();  // o prazo conta desde o último byte (Stop pode ter 100 KB)
    s_req.feed(buf, (size_t)n, on_body, nullptr);
    // Já no fim dos cabeçalhos, sem ler o corpo: grande demais ou chave errada.
    if (s_req.state() == HttpRequest::State::Body) {
      if (s_req.content_length() > kMaxBody) {
        LOG_I(TAG, "corpo de %ld B recusado (máx %ld)", (long)s_req.content_length(), (long)kMaxBody);
        reply(413, "Payload Too Large");
        return;
      }
      if (needs_key(s_req.path()) && strcmp(s_req.bearer(), settings().hook_key) != 0) {
        LOG_I(TAG, "chave errada de %s", s_client.remoteIP().toString().c_str());
        reply(401, "Unauthorized");
        return;
      }
    }
  }
  if (s_req.failed()) {
    reply(400, "Bad Request");
  } else if (s_req.done()) {
    finish();
  } else if (millis() - s_since > kClientTimeoutMs) {
    LOG_I(TAG, "cliente parado, fechando");
    s_client.stop();
    s_busy = false;
  }
}
