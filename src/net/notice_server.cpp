#include "notice_server.h"

#include <WiFi.h>
#include <errno.h>
#include <lwip/sockets.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "data/Settings.h"
#include "data/SaveScratch.h"
#include "data/StatusStore.h"
#include "data/paths.h"
#include "hal/sd.h"
#include "logic/Sun.h"
#include "logic/WakeLease.h"
#include "net/HttpRequest.h"
#include "notify/NoticeJson.h"
#include "saves/SaveFormat.h"
#include "saves/SaveName.h"
#include "ui/catalog.h"
#include "ui/nav.h"
#include "ui/power.h"
#include "util/log.h"
#include "version.h"

static const char *TAG = "notices";
// 3 s sem chegar nada: cliente parado, fecha (senão segura o servidor).
static const uint32_t kClientTimeoutMs = 3000;
// No upload o TCP pode estar esperando retransmissão (pacote perdido ou
// descartado por falta de heap): dá mais tempo antes de desistir.
static const uint32_t kUploadTimeoutMs = 10000;
// Corpo maior que isso: 413 sem ler. Um body grande chegando mais rápido que
// o loop lê enchia os buffers do TCP e o Wi-Fi perdia pacotes. Um aviso
// cabe com folga em 1 KB.
static const int32_t kMaxBody = 4 * 1024;
static const char kNotices[] = "/notices";
static const size_t kNoticesLen = sizeof(kNotices) - 1;
static const char kStatus[] = "/status";
static const size_t kStatusLen = sizeof(kStatus) - 1;
static const char kSaves[] = "/saves";
static const size_t kSavesLen = sizeof(kSaves) - 1;
// Gen3 tem 128 KB, Gen4 512 KB. Vai em fluxo pro SD: o limite não pesa na RAM.
static const int32_t kMaxSave = 1024 * 1024;
// O upload grava aqui e só renomeia no fim: corpo incompleto não estraga o save.
static const char kUploadTmp[] = SAVES_DIR "/.upload.tmp";
// Saves listados no GET /saves (o mesmo limite do StatusStore).
static const uint8_t kMaxListed = 16;

static WiFiServer *s_server = nullptr;
static WiFiClient s_client;
static bool s_busy = false;
static bool s_closing = false;  // resposta enviada, esperando o cliente fechar
static uint32_t s_close_at = 0;
static const uint32_t kCloseWaitMs = 2000;
static uint32_t s_since = 0;
static HttpRequest s_req;
static NoticeJson s_json;
static NoticeBoard s_board;
static StatusBoard s_status;
static uint8_t s_arrivals = 0;

// O que fazer com o corpo, decidido no primeiro pedaço (os cabeçalhos já chegaram).
enum class BodyMode : uint8_t { Unset, Json, Upload, Discard };
static BodyMode s_body = BodyMode::Unset;
static bool s_upload_failed = false;  // cartão cheio / erro de escrita


NoticeBoard &notices() { return s_board; }
StatusBoard &statuses() { return s_status; }

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

static void close_now();

void notice_server_stop() {
  if (!s_server) return;
  if (s_busy) close_now();
  s_busy = false;
  s_server->end();
  delete s_server;
  s_server = nullptr;
}

// Upload pela metade (erro, 401/413, cliente sumiu): apaga o temporário.
static void upload_abort() {
  if (!sd_is_writing()) return;
  sd_close_write();
  sd_remove(kUploadTmp);
  LOG_I(TAG, "upload interrompido");
}

// O connected() do WiFiClient não vê o FIN (recv de 0 byte sempre dá 0): um
// cliente que desistiu segurava o servidor até o prazo. recv de 1 byte com
// MSG_PEEK devolve 0 quando o outro lado fechou.
static bool peer_closed() {
  uint8_t b;
  int r = recv(s_client.fd(), &b, 1, MSG_PEEK | MSG_DONTWAIT);
  if (r == 0) return true;
  return r < 0 && errno != EAGAIN && errno != EWOULDBLOCK;
}

// Fecha com RST (SO_LINGER 0): sem TIME_WAIT. No ESP-IDF o lwIP aloca o PCB
// do heap, e cada conexão que o CYD fechava primeiro deixava um bloco pequeno
// preso por ~2 min. Com muitas conexões seguidas o heap picotava abaixo dos
// ~1,7 KB que o driver do Wi-Fi pede por pacote, e o link morria calado
// (associado, mas sem passar nada). Só pra quando o cliente sumiu ou já leu
// tudo: o RST descarta o que ainda não saiu.
static void close_now() {
  if (s_client.fd() >= 0) {
    struct linger lg = {1, 0};
    setsockopt(s_client.fd(), SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
  }
  s_client.stop();
  s_busy = false;
  s_closing = false;
}

static void drop_client() {
  upload_abort();
  close_now();
}

// Resposta inteira no socket: espera o cliente fechar primeiro (ele fica com o
// TIME_WAIT). Toda resposta diz o tamanho (Content-Length ou chunked), então
// curl e o app fecham assim que leem tudo.
static void begin_close() {
  s_client.flush();
  s_closing = true;
  s_close_at = millis();
}

static void close_step() {
  if (peer_closed() || millis() - s_close_at > kCloseWaitMs) close_now();
}

static void reply(int code, const char *reason, const char *body = nullptr) {
  upload_abort();
  char head[160];
  size_t blen = body ? strlen(body) : 0;
  int n = snprintf(head, sizeof(head),
                   "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Length: %u\r\n%s\r\n", code,
                   reason, (unsigned)blen, body ? "Content-Type: application/json\r\n" : "");
  s_client.write((const uint8_t *)head, n);
  if (blen) s_client.write((const uint8_t *)body, blen);
  begin_close();
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

// /status ou /status/<id>.
static bool status_path(const char *path, const char **id) {
  if (strncmp(path, kStatus, kStatusLen) != 0) return false;
  const char *rest = path + kStatusLen;
  if (!*rest) {
    *id = rest;
    return true;
  }
  if (*rest != '/' || !rest[1]) return false;
  *id = rest + 1;
  return true;
}

// /saves ou /saves/<nome em %XX>. `name` aponta pro que vem depois da '/'.
static bool saves_path(const char *path, const char **name) {
  if (strncmp(path, kSaves, kSavesLen) != 0) return false;
  const char *rest = path + kSavesLen;
  if (!*rest) {
    *name = rest;
    return true;
  }
  if (*rest != '/' || !rest[1]) return false;
  *name = rest + 1;
  return true;
}

// Tudo menos o GET / pede a chave.
static bool needs_key(const char *path) {
  const char *id;
  return notices_path(path, &id) || status_path(path, &id) || saves_path(path, &id) ||
         strcmp(path, "/dex") == 0 ||
         strcmp(path, "/location") == 0 || strcmp(path, "/sleep") == 0 ||
         strcmp(path, "/wake") == 0;
}

static bool authorized() { return strcmp(s_req.bearer(), settings().hook_key) == 0; }

static bool is_upload() {
  const char *name;
  return strcmp(s_req.method(), "PUT") == 0 && saves_path(s_req.path(), &name) && *name;
}

static int32_t max_body() { return is_upload() ? kMaxSave : kMaxBody; }

static BodyMode start_body() {
  if (!is_upload()) return BodyMode::Json;
  const char *raw;
  saves_path(s_req.path(), &raw);
  char name[SAVE_NAME_MAX + 1];
  if (!authorized() || s_req.content_length() > kMaxSave || !save_name_decode(raw, name, sizeof(name)) ||
      !sd_is_present()) {
    return BodyMode::Discard;  // o finish (ou o 401/413) responde
  }
  sd_close_read();  // max_files 3: o leitor de sprites não precisa ficar aberto
  if (!sd_mkdir(SAVES_DIR) || !sd_open_write(kUploadTmp)) {
    s_upload_failed = true;
    return BodyMode::Discard;
  }
  return BodyMode::Upload;
}

static void on_body(void *, const char *data, size_t len) {
  if (s_body == BodyMode::Unset) s_body = start_body();
  switch (s_body) {
    case BodyMode::Json: s_json.feed(data, len); break;
    case BodyMode::Upload:
      if (!s_upload_failed && !sd_write((const uint8_t *)data, len)) s_upload_failed = true;
      break;
    default: break;
  }
}

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
  uint32_t rev = 0;
  PostResult r = s_board.post(n, millis(), &rev);
  if (r == PostResult::Added) {
    s_arrivals |= (n.wake ? NOTICE_ARRIVAL_WAKE : 0) | (n.sound ? NOTICE_ARRIVAL_SOUND : 0);
  }
  LOG_I(TAG, "aviso %s \"%s\"%s", n.id, n.title,
        r == PostResult::Repeated ? " (repetido)" : r == PostResult::Dropped ? " (quadro cheio)" : "");
  char body[80];
  size_t len = 0;
  memcpy(body, "{\"id\":", 6);
  len = 6;
  json_str(body, sizeof(body) - 16, &len, n.id);
  snprintf(body + len, sizeof(body) - len, ",\"rev\":%lu}", (unsigned long)rev);
  reply(201, "Created", body);
}

// GET /notices/<id>: o hook que espera a resposta pergunta aqui a cada 1 s.
static void get_notice(const char *id) {
  uint32_t now = millis();
  uint32_t rev = 0;
  int i = s_board.find(id);
  NoticeAnswer a = NoticeAnswer::None;
  if (i >= 0) rev = s_board.rev((uint8_t)i);
  else a = s_board.answered(id, now, &rev);
  if (i < 0 && a == NoticeAnswer::None) {
    reply(404, "Not Found");
    return;
  }
  char body[96];
  size_t len = 6;
  memcpy(body, "{\"id\":", 6);
  json_str(body, sizeof(body) - 40, &len, id);
  if (a == NoticeAnswer::None)
    snprintf(body + len, sizeof(body) - len, ",\"rev\":%lu,\"answer\":null}", (unsigned long)rev);
  else
    snprintf(body + len, sizeof(body) - len, ",\"rev\":%lu,\"answer\":\"%s\"}", (unsigned long)rev,
             notice_answer_name(a));
  reply(200, "OK", body);
}

// PUT /status/<id> (corpo JSON), DELETE /status[/<id>], GET /status.
static void status_route(const char *id, bool is_get, bool is_delete) {
  bool is_put = strcmp(s_req.method(), "PUT") == 0;
  if (!*id) {
    if (is_delete) {
      s_status.remove_all();
      reply(204, "No Content");
    } else if (is_get) {
      static char body[256];
      size_t n = 0;
      body[n++] = '[';
      uint32_t now = millis();
      for (uint8_t i = 0; i < s_status.size(); i++) {
        const StatusItem &it = s_status.at(i);
        if (i) body[n++] = ',';
        n += snprintf(body + n, sizeof(body) - n, "{\"id\":");
        json_str(body, sizeof(body) - 2, &n, it.id);
        n += snprintf(body + n, sizeof(body) - n, ",\"text\":");
        json_str(body, sizeof(body) - 2, &n, it.text);
        n += snprintf(body + n, sizeof(body) - n, ",\"age\":%lu}",
                      (unsigned long)((now - s_status.set_ms(i)) / 1000));
      }
      body[n++] = ']';
      body[n] = '\0';
      reply(200, "OK", body);
    } else {
      reply(405, "Method Not Allowed");
    }
    return;
  }
  if (is_delete) {
    s_status.remove(id);
    reply(204, "No Content");
    return;
  }
  if (!is_put) {
    reply(405, "Method Not Allowed");
    return;
  }
  if (!s_json.done()) {
    reply_error("json");
    return;
  }
  StatusItem it;
  StatusError err = status_from_fields(s_json.fields(), id, &it);
  if (err == StatusError::None) err = s_status.put(it, millis());
  if (err == StatusError::Full) {
    reply(409, "Conflict", "{\"error\":\"full\"}");
  } else if (err != StatusError::None) {
    reply_error(status_error_field(err));
  } else {
    reply(204, "No Content");
  }
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

// Resposta em pedaços (chunked), sem montar o corpo inteiro na RAM. O cliente
// sabe onde acaba e fecha primeiro (ver begin_close).
static void reply_chunked_head(const char *type) {
  char head[128];
  int n = snprintf(head, sizeof(head),
                   "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: %s\r\n"
                   "Transfer-Encoding: chunked\r\n\r\n",
                   type);
  s_client.write((const uint8_t *)head, n);
}

static void write_chunk(const void *data, size_t len) {
  if (!len) return;
  char size[12];
  int n = snprintf(size, sizeof(size), "%x\r\n", (unsigned)len);
  s_client.write((const uint8_t *)size, n);
  s_client.write((const uint8_t *)data, len);
  s_client.write((const uint8_t *)"\r\n", 2);
}

static void reply_chunked_end() {
  s_client.write((const uint8_t *)"0\r\n\r\n", 5);
  begin_close();
}

struct SaveEntry {
  char name[SAVE_NAME_MAX + 1];
  uint32_t size;
  uint32_t mtime;
};

struct SaveListing {
  SaveEntry *items;
  uint8_t count;
};

// Só copia. Não abrir arquivo aqui (diretório aberto trava a lib SD).
static void collect_save(const char *name, size_t size, uint32_t mtime, void *user) {
  SaveListing *l = (SaveListing *)user;
  // "._x.sav" é o AppleDouble que o macOS cria no cartão: não é save.
  if (l->count >= kMaxListed || name[0] == '.' || strlen(name) > SAVE_NAME_MAX) return;
  SaveEntry &e = l->items[l->count++];
  strcpy(e.name, name);
  e.size = (uint32_t)size;
  e.mtime = mtime;
}

static bool sd_reader(size_t off, uint8_t *out, size_t len, void *) { return sd_read_at(off, out, len); }

// GET /saves: [{"name","size","mtime","format"}]. format = o parser que
// reconheceu o save (null = desconhecido ou corrompido).
static void list_saves() {
  if (!sd_is_present()) {
    reply(503, "Service Unavailable");
    return;
  }
  // ~1,1 KB só durante a resposta.
  SaveListing l{(SaveEntry *)malloc(sizeof(SaveEntry) * kMaxListed), 0};
  if (!l.items) {
    reply(503, "Service Unavailable");
    return;
  }
  sd_list_files(SAVES_DIR, ".sav", collect_save, &l);
  sd_list_files(SAVES_DIR, ".srm", collect_save, &l);

  reply_chunked_head("application/json");
  write_chunk("[", 1);
  static DexStatus scratch;  // fora da pilha (o parse Gen3 já usa ~12 KB dela)
  for (uint8_t i = 0; i < l.count; i++) {
    const SaveEntry &e = l.items[i];
    char path[sizeof(kSaves) + SAVE_NAME_MAX + 1];
    snprintf(path, sizeof(path), "%s/%s", SAVES_DIR, e.name);
    const char *fmt = nullptr;
    size_t size = 0;
    if (sd_open_read(path, &size)) {
      SaveReader r{size, sd_reader, nullptr, save_scratch()};
      scratch.clear();
      fmt = save_merge_dex(r, scratch);
      sd_close_read();
    }
    char item[sizeof(SaveEntry::name) * 2 + 96];
    size_t n = 0;
    if (i) item[n++] = ',';
    memcpy(item + n, "{\"name\":", 8);
    n += 8;
    json_str(item, sizeof(item) - 48, &n, e.name);
    n += snprintf(item + n, sizeof(item) - n, ",\"size\":%lu,\"mtime\":%lu,\"format\":",
                  (unsigned long)e.size, (unsigned long)e.mtime);
    if (fmt) json_str(item, sizeof(item) - 2, &n, fmt);
    else n += snprintf(item + n, sizeof(item) - n, "null");
    item[n++] = '}';
    write_chunk(item, n);
  }
  write_chunk("]", 1);
  free(l.items);
  reply_chunked_end();
}


static void put_save(const char *path) {
  if (s_req.content_length() <= 0) {
    reply(411, "Length Required");
    return;
  }
  if (s_body != BodyMode::Upload || s_upload_failed) {
    LOG_E(TAG, "upload de %s falhou", path);
    reply(507, "Insufficient Storage");
    return;
  }
  sd_close_write();
  if (!sd_rename(kUploadTmp, path)) {
    sd_remove(kUploadTmp);
    reply(507, "Insufficient Storage");
    return;
  }
  LOG_I(TAG, "save recebido: %s (%ld B)", path, (long)s_req.content_length());
  reply(201, "Created");
}

static void reload_saves() {
  if (!sd_is_present()) {
    reply(503, "Service Unavailable");
    return;
  }
  // O mesmo do Ajustes › Sistema › Recarregar, sem passar pela tela do Boot.
  StatusLoadReport rep = status_store_load(true, nullptr, nullptr);
  catalog_invalidate();
  Screen cur = nav_current();
  if (cur != Screen::Boot && cur != Screen::SdError) nav_reload();
  const DexStatus &st = status_store_get();
  char body[96];
  snprintf(body, sizeof(body), "{\"saves\":%u,\"parsed\":%u,\"seen\":%u,\"caught\":%u}",
           rep.saves_found, rep.saves_parsed, st.count_seen(), st.count_caught());
  reply(200, "OK", body);
}

static void saves_route(const char *raw, bool is_get, bool is_post, bool is_delete) {
  if (!*raw) {
    if (is_get) list_saves();
    else reply(405, "Method Not Allowed");
    return;
  }
  if (strcmp(raw, "reload") == 0) {
    if (is_post) reload_saves();
    else reply(405, "Method Not Allowed");
    return;
  }
  char name[SAVE_NAME_MAX + 1];
  if (!save_name_decode(raw, name, sizeof(name))) {
    reply_error("name");
    return;
  }
  if (!sd_is_present()) {
    reply(503, "Service Unavailable");
    return;
  }
  char path[sizeof(kSaves) + SAVE_NAME_MAX + 1];
  snprintf(path, sizeof(path), "%s/%s", SAVES_DIR, name);
  // Sem GET /saves/<nome> (download) na v1: mandar centenas de KB seguidos
  // derrubava o link (o CYD seguia associado, mas parava de passar pacotes).
  if (strcmp(s_req.method(), "PUT") == 0) {
    put_save(path);
  } else if (is_delete) {
    sd_close_read();
    bool ok = sd_remove(path);
    LOG_I(TAG, "save apagado: %s%s", path, ok ? "" : " (falhou)");
    if (ok) reply(204, "No Content");
    else reply(500, "Internal Server Error");
  } else {
    reply(405, "Method Not Allowed");
  }
}

// GET /dex: bits visto/capturado (bit = n-1) em hex, como no DexStatus.
static void get_dex() {
  const DexStatus &st = status_store_get();
  static char body[2 * 2 * DEX_BITS_BYTES + 96];
  static const char hex[] = "0123456789abcdef";
  size_t n = (size_t)snprintf(body, sizeof(body), "{\"count\":%d,\"seen\":\"", DEX_COUNT);
  for (int i = 0; i < DEX_BITS_BYTES; i++) {
    body[n++] = hex[st.seen[i] >> 4];
    body[n++] = hex[st.seen[i] & 15];
  }
  memcpy(body + n, "\",\"caught\":\"", 12);
  n += 12;
  for (int i = 0; i < DEX_BITS_BYTES; i++) {
    body[n++] = hex[st.caught[i] >> 4];
    body[n++] = hex[st.caught[i] & 15];
  }
  body[n++] = '"';
  body[n++] = '}';
  body[n] = '\0';
  reply(200, "OK", body);
}

// GET /location: {"lat","lon","tz","night","rise","set"} ({} sem localidade).
// rise/set em "HH:MM" no fuso dado; null sem hora (NTP) ou no dia/noite polar.
static void get_location() {
  const AppSettings &st = settings();
  if (!st.has_location) {
    reply(200, "OK", "{}");
    return;
  }
  char rise[8] = "null", set[8] = "null", night[6] = "null";
  time_t now = time(nullptr);
  if ((int64_t)now >= SUN_VALID_TIME) {
    strcpy(night, sun_is_night((int64_t)now, st.lat, st.lon) ? "true" : "false");
    SunDay d = sun_day((int64_t)now, st.lat, st.lon);
    if (d.kind == SunKind::Normal) {
      char t[6];
      sun_format_hhmm(d.rise, st.tz_min, t, sizeof(t));
      snprintf(rise, sizeof(rise), "\"%s\"", t);
      sun_format_hhmm(d.set, st.tz_min, t, sizeof(t));
      snprintf(set, sizeof(set), "\"%s\"", t);
    }
  }
  char body[128];
  snprintf(body, sizeof(body), "{\"lat\":%.4f,\"lon\":%.4f,\"tz\":%d,\"night\":%s,\"rise\":%s,\"set\":%s}",
           st.lat, st.lon, st.tz_min, night, rise, set);
  reply(200, "OK", body);
}

static void location_route(bool is_get, bool is_delete) {
  bool is_put = strcmp(s_req.method(), "PUT") == 0;
  if (is_get) {
    get_location();
    return;
  }
  AppSettings &st = settings();
  if (is_delete) {
    st.has_location = false;
    settings_save_location();
    power_location_changed();
    LOG_I(TAG, "localidade apagada");
    reply(204, "No Content");
    return;
  }
  if (!is_put) {
    reply(405, "Method Not Allowed");
    return;
  }
  float lat, lon;
  int16_t tz;
  const char *bad = sun_parse_location(s_req.query(), &lat, &lon, &tz);
  if (bad) {
    reply_error(bad);
    return;
  }
  // O app reenvia a cada mudança de fuso; só grava se mudou.
  bool changed = !st.has_location || st.lat != lat || st.lon != lon || st.tz_min != tz;
  st.has_location = true;
  st.lat = lat;
  st.lon = lon;
  st.tz_min = tz;
  if (changed) {
    settings_save_location();
    LOG_I(TAG, "localidade %.4f,%.4f tz=%d", lat, lon, tz);
  }
  power_location_changed();
  reply(204, "No Content");
}

static void finish() {
  const char *method = s_req.method();
  const char *path = s_req.path();
  bool is_get = strcmp(method, "GET") == 0;
  bool is_post = strcmp(method, "POST") == 0;
  bool is_delete = strcmp(method, "DELETE") == 0;

  if (strcmp(path, "/") == 0 && is_get) {
    // api 2: /saves e /dex; api 3: /location; api 4: /status e resposta nos
    // avisos (actions, GET /notices/<id>); api 5: /wake?ttl. Os clientes
    // conferem antes de usar.
    char body[96];
    snprintf(body, sizeof(body), "{\"name\":\"CYDex\",\"version\":\"" CYDEX_VERSION "\",\"api\":5,\"rssi\":%d}",
             (int)WiFi.RSSI());
    reply(200, "OK", body);
    return;
  }
  if (!needs_key(path)) {
    reply(404, "Not Found");
    return;
  }
  if (!authorized()) {
    LOG_I(TAG, "chave errada de %s", s_client.remoteIP().toString().c_str());
    reply(401, "Unauthorized");
    return;
  }

  if (strcmp(path, "/dex") == 0) {
    if (is_get) get_dex();
    else reply(405, "Method Not Allowed");
    return;
  }
  if (strcmp(path, "/location") == 0) {
    location_route(is_get, is_delete);
    return;
  }

  const char *id;
  if (status_path(path, &id)) {
    if (strlen(id) >= sizeof(StatusItem::id)) reply_error("id");
    else status_route(id, is_get, is_delete);
    return;
  }
  if (saves_path(path, &id)) {
    saves_route(id, is_get, is_post, is_delete);
    return;
  }
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
    if (strlen(id) >= sizeof(Notice::id)) {
      reply_error("id");
      return;
    }
    if (is_get) {
      get_notice(id);
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
  uint32_t ttl = 0;
  if (!sleep) {
    const char *bad = wake_lease_parse(s_req.query(), &ttl);
    if (bad) {
      reply_error(bad);
      return;
    }
  }
  if (sleep) {
    LOG_I(TAG, "computador dormiu");
    power_remote_sleep();
  } else if (ttl) {
    // A renovação a cada status não vai pro log (só o /wake de verdade).
    if (power_remote_keepalive(ttl)) LOG_I(TAG, "computador acordou (prazo %u s)", (unsigned)ttl);
  } else {
    LOG_I(TAG, "computador acordou");
    power_remote_wake();
  }
  reply(204, "No Content");
}

void notice_server_tick() {
  if (!s_server) return;
  if (s_busy && s_closing) {
    close_step();
    return;
  }
  if (!s_busy) {
    s_client = s_server->available();
    if (!s_client) return;
    s_busy = true;
    s_since = millis();
    s_req.reset();
    s_json.reset();
    s_body = BodyMode::Unset;
    s_upload_failed = false;
  }
  if (!s_client.connected() && !s_client.available()) {
    drop_client();
    return;
  }
  // Lê o que já chegou, em pedaços de 256 B (pilha), e devolve o loop.
  char buf[256];
  // Até 8 KB por volta do loop. No upload, 64 KB: cada pacote parado no
  // socket segura heap que o driver do Wi-Fi precisa pra receber o próximo.
  int budget = s_body == BodyMode::Upload ? 256 : 32;
  while (s_client.available() && budget-- > 0 && !s_req.done() && !s_req.failed()) {
    int n = s_client.read((uint8_t *)buf, sizeof(buf));
    if (n <= 0) break;
    s_since = millis();  // o prazo conta desde o último byte
    s_req.feed(buf, (size_t)n, on_body, nullptr);
    // Já no fim dos cabeçalhos, sem ler o corpo: grande demais ou chave errada.
    if (s_req.state() == HttpRequest::State::Body) {
      if (s_req.content_length() > max_body()) {
        LOG_I(TAG, "corpo de %ld B recusado (máx %ld)", (long)s_req.content_length(), (long)max_body());
        reply(413, "Payload Too Large");
        return;
      }
      if (needs_key(s_req.path()) && !authorized()) {
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
  } else if (millis() - s_since > (s_body == BodyMode::Upload ? kUploadTimeoutMs : kClientTimeoutMs)) {
    LOG_I(TAG, "cliente parado, fechando");
    drop_client();
  }
}
