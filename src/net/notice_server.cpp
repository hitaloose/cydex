#include "notice_server.h"

#include <WiFi.h>

#include <cstdio>
#include <cstring>

#include "data/Settings.h"
#include "net/HttpRequest.h"
#include "notify/NoticeJson.h"
#include "ui/power.h"
#include "util/log.h"
#include "version.h"

static const char *TAG = "notices";
// 3 s sem chegar nada: cliente parado, fecha (senão segura o servidor).
static const uint32_t kClientTimeoutMs = 3000;
// Corpo maior que isso: 413 sem ler. Um body grande chegando mais rápido que
// o loop lê enchia os buffers do TCP e o Wi-Fi perdia pacotes. Um aviso
// cabe com folga em 1 KB.
static const int32_t kMaxBody = 4 * 1024;
static const char kNotices[] = "/notices";
static const size_t kNoticesLen = sizeof(kNotices) - 1;

static WiFiServer *s_server = nullptr;
static WiFiClient s_client;
static bool s_busy = false;
static uint32_t s_since = 0;
static HttpRequest s_req;
static NoticeJson s_json;
static NoticeBoard s_board;
static uint8_t s_arrivals = 0;

NoticeBoard &notices() { return s_board; }

uint8_t notice_take_arrivals() {
  uint8_t a = s_arrivals;
  s_arrivals = 0;
  return a;
}

void notice_server_start() {
  if (s_server) return;
  // new uma vez: o WiFiServer fica vivo enquanto conectado (~100 B).
  s_server = new WiFiServer(80);
  s_server->begin();
  s_server->setNoDelay(true);
  LOG_I(TAG, "ouvindo na porta 80");
}

void notice_server_stop() {
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

// /notices ou /notices/<id>. `id` aponta pro que vem depois da '/' ("" = a coleção).
static bool notices_path(const char *path, const char **id) {
  if (strncmp(path, kNotices, kNoticesLen) != 0) return false;
  const char *rest = path + kNoticesLen;
  if (!*rest) {
    *id = rest;
    return true;
  }
  if (*rest != '/' || !rest[1]) return false;
  *id = rest + 1;
  return true;
}

// Tudo menos o GET / pede a chave.
static bool needs_key(const char *path) {
  const char *id;
  return notices_path(path, &id) || strcmp(path, "/sleep") == 0 || strcmp(path, "/wake") == 0;
}

static void on_body(void *, const char *data, size_t len) { s_json.feed(data, len); }

// Escreve `s` como string JSON (com aspas) em out[*n], sem passar de cap.
static void json_str(char *out, size_t cap, size_t *n, const char *s) {
  auto put = [&](char ch) {
    if (*n + 1 < cap) out[(*n)++] = ch;
  };
  put('"');
  for (; *s; s++) {
    unsigned char ch = (unsigned char)*s;
    if (ch == '"' || ch == '\\') {
      put('\\');
      put((char)ch);
    } else if (ch < 0x20) {
      char esc[8];
      snprintf(esc, sizeof(esc), "\\u%04x", ch);
      for (char *e = esc; *e; e++) put(*e);
    } else {
      put((char)ch);
    }
  }
  put('"');
  out[*n] = '\0';
}

static void reply_error(const char *field) {
  char body[48];
  snprintf(body, sizeof(body), "{\"error\":\"%s\"}", field);
  reply(400, "Bad Request", body);
}

static void post_notice() {
  if (!s_json.done()) {
    reply_error("json");
    return;
  }
  Notice n;
  NoticeError err = notice_from_fields(s_json.fields(), &n);
  if (err != NoticeError::None) {
    LOG_I(TAG, "recusado: campo %s", notice_error_field(err));
    reply_error(notice_error_field(err));
    return;
  }
  PostResult r = s_board.post(n, millis());
  if (r == PostResult::Added) {
    s_arrivals |= (n.wake ? NOTICE_ARRIVAL_WAKE : 0) | (n.sound ? NOTICE_ARRIVAL_SOUND : 0);
  }
  LOG_I(TAG, "aviso %s \"%s\"%s", n.id, n.title,
        r == PostResult::Repeated ? " (repetido)" : r == PostResult::Dropped ? " (quadro cheio)" : "");
  char body[64];
  size_t len = 0;
  memcpy(body, "{\"id\":", 6);
  len = 6;
  json_str(body, sizeof(body) - 1, &len, n.id);
  body[len++] = '}';
  body[len] = '\0';
  reply(201, "Created", body);
}

static void list_notices() {
  // 4 avisos x ~200 B no pior caso (título com escapes). Estático: fora da pilha.
  static char body[1024];
  size_t n = 0;
  body[n++] = '[';
  uint32_t now = millis();
  for (uint8_t i = 0; i < s_board.size(); i++) {
    const Notice &it = s_board.at(i);
    char num[48];
    if (i) body[n++] = ',';
    n += snprintf(body + n, sizeof(body) - n, "{\"id\":");
    json_str(body, sizeof(body) - 2, &n, it.id);
    n += snprintf(body + n, sizeof(body) - n, ",\"tag\":");
    json_str(body, sizeof(body) - 2, &n, it.tag);
    n += snprintf(body + n, sizeof(body) - n, ",\"title\":");
    json_str(body, sizeof(body) - 2, &n, it.title);
    snprintf(num, sizeof(num), ",\"type\":\"%s\",\"age\":%lu}", notice_type_name(it),
             (unsigned long)((now - s_board.posted_ms(i)) / 1000));
    size_t k = strlen(num);
    if (n + k + 2 < sizeof(body)) {
      memcpy(body + n, num, k);
      n += k;
    }
  }
  body[n++] = ']';
  body[n] = '\0';
  reply(200, "OK", body);
}

static void finish() {
  const char *method = s_req.method();
  const char *path = s_req.path();
  bool is_get = strcmp(method, "GET") == 0;
  bool is_post = strcmp(method, "POST") == 0;
  bool is_delete = strcmp(method, "DELETE") == 0;

  if (strcmp(path, "/") == 0 && is_get) {
    reply(200, "OK", "{\"name\":\"CYDex\",\"version\":\"" CYDEX_VERSION "\"}");
    return;
  }
  if (!needs_key(path)) {
    reply(404, "Not Found");
    return;
  }
  if (strcmp(s_req.bearer(), settings().hook_key) != 0) {
    LOG_I(TAG, "chave errada de %s", s_client.remoteIP().toString().c_str());
    reply(401, "Unauthorized");
    return;
  }

  const char *id;
  if (notices_path(path, &id)) {
    if (!*id) {
      if (is_post) {
        post_notice();
      } else if (is_get) {
        list_notices();
      } else if (is_delete) {
        uint8_t removed = s_board.remove_all();
        LOG_I(TAG, "fecha todos (%u)", removed);
        reply(204, "No Content");
      } else {
        reply(405, "Method Not Allowed");
      }
      return;
    }
    if (!is_delete) {
      reply(405, "Method Not Allowed");
      return;
    }
    char tag[sizeof(Notice::tag)] = "";
    if (s_req.query()[0] && !http_query_param(s_req.query(), "tag", tag, sizeof(tag)) &&
        strstr(s_req.query(), "tag=")) {
      reply_error("tag");  // não coube
      return;
    }
    uint8_t removed = s_board.remove(id, tag);
    LOG_I(TAG, "fecha %s%s%s (%u)", id, tag[0] ? " tag " : "", tag, removed);
    reply(204, "No Content");
    return;
  }

  // /sleep, /wake
  if (!is_post) {
    reply(405, "Method Not Allowed");
    return;
  }
  bool sleep = path[1] == 's';
  LOG_I(TAG, "computador %s", sleep ? "dormiu" : "acordou");
  if (sleep) power_remote_sleep();
  else power_remote_wake();
  reply(204, "No Content");
}

void notice_server_tick() {
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
    s_since = millis();  // o prazo conta desde o último byte
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
