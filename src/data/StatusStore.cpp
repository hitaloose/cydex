#include "StatusStore.h"

#include "SaveStore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dex/StatusCache.h"
#include "hal/sd.h"
#include "paths.h"
#include "saves/SaveFormat.h"
#include "saves/SavesCache.h"
#include "util/log.h"

static const char *TAG = "status";

namespace {

constexpr uint8_t kMaxSaves = 16;
// Nomes no padrão no-intro passam de 48 caracteres (achado no PokEsp).
constexpr size_t kMaxName = 128;

struct SaveList {
  char names[kMaxSaves][kMaxName];
  uint8_t count;
  SaveSignature sig;
};

DexStatus s_status;
SaveList s_list;  // estático: ~2 KB fora da pilha

// Só copia o nome. NÃO abrir arquivo aqui: o diretório ainda está aberto,
// e abrir outro arquivo nesse momento trava a lib SD (achado no PokEsp).
void collect(const char *name, size_t size, uint32_t mtime, void *user) {
  SaveList *l = (SaveList *)user;
  // "._x.sav" é o AppleDouble que o macOS cria no cartão: não é save.
  if (l->count >= kMaxSaves || name[0] == '.') return;
  strncpy(l->names[l->count], name, kMaxName - 1);
  l->names[l->count][kMaxName - 1] = '\0';
  l->count++;
  l->sig.add(name, (uint32_t)size, mtime);
}

bool sd_reader(size_t off, uint8_t *out, size_t len, void *) { return sd_read_at(off, out, len); }

// O saves.cache vale pra esta lista de saves?
bool saves_cache_matches(const SaveSignature &sig) {
  size_t size = 0;
  if (!sd_open_read(SAVES_CACHE_PATH, &size)) return false;
  uint8_t h[SAVES_CACHE_HEADER_SIZE];
  uint16_t count = 0;
  bool ok = size == SAVES_CACHE_HEADER_SIZE + (size_t)sig.count * SAVES_CACHE_RECORD_SIZE &&
            sd_read_at(0, h, sizeof(h)) && saves_cache_header_decode(h, sig, &count);
  sd_close_read();
  return ok;
}

// Resumo de um save (o arquivo já aberto pra leitura). Estáticos: a cadeia
// boot -> parser -> SD é a que estourou a pilha no PokEsp.
void summarize_into(const char *name, size_t size, SaveEntry *e) {
  memset(e, 0, sizeof(*e));
  strncpy(e->file, name, sizeof(e->file) - 1);
  SaveReader r{size, sd_reader, nullptr, save_scratch()};
  SaveHandle h;
  e->ok = save_open(r, &h) && h.format->summarize(h, e->summary);
}

}  // namespace

static StatusLoadReport load_impl(bool force, StatusProgressCb cb, void *user) {
  StatusLoadReport rep{};
  s_status.clear();
  memset(&s_list, 0, sizeof(s_list));
  s_list.sig = SaveSignature();

  sd_list_files(SAVES_DIR, ".sav", collect, &s_list);
  sd_list_files(SAVES_DIR, ".srm", collect, &s_list);
  rep.saves_found = s_list.count;

  if (!force) {
    static uint8_t cache[STATUS_CACHE_SIZE];
    if (sd_read_whole(STATUS_CACHE_PATH, cache, sizeof(cache)) &&
        status_cache_decode(cache, sizeof(cache), s_list.sig, &s_status) &&
        saves_cache_matches(s_list.sig)) {
      rep.from_cache = true;
      rep.saves_parsed = s_list.count;
      if (cb) cb(s_list.count, s_list.count, user);
      LOG_I(TAG, "cache ok — %u saves, %u vistos, %u capturados", s_list.count,
            s_status.count_seen(), s_status.count_caught());
      return rep;
    }
  }

  // Buffers do saves.cache no heap só durante o parse (~2 KB; a RAM estática é curta).
  uint8_t hdr[SAVES_CACHE_HEADER_SIZE];
  uint8_t *rec = (uint8_t *)malloc(SAVES_CACHE_RECORD_SIZE);
  SaveEntry *entry_p = (SaveEntry *)malloc(sizeof(SaveEntry));
  if (!rec || !entry_p) {
    LOG_E(TAG, "sem memória pro saves.cache");
    free(rec);
    free(entry_p);
    rec = nullptr;
    entry_p = nullptr;
  }
  // Sem memória, o laço segue (a dex sai igual) e só o saves.cache fica sem gravar.
  SaveEntry *entry = entry_p;
  bool cache_ok = rec && sd_open_write(SAVES_CACHE_PATH);
  saves_cache_header_encode(s_list.sig, hdr);
  if (cache_ok) cache_ok = sd_write(hdr, sizeof(hdr));

  for (uint8_t i = 0; i < s_list.count; i++) {
    char path[kMaxName + 16];
    snprintf(path, sizeof(path), "%s/%s", SAVES_DIR, s_list.names[i]);
    size_t size = 0;
    if (entry) {
      memset(entry, 0, sizeof(*entry));
      strncpy(entry->file, s_list.names[i], sizeof(entry->file) - 1);
    }
    if (sd_open_read(path, &size)) {
      SaveReader r{size, sd_reader, nullptr, save_scratch()};
      const char *fmt = save_merge_dex(r, s_status);
      if (fmt && entry) summarize_into(s_list.names[i], size, entry);
      sd_close_read();
      if (fmt) {
        rep.saves_parsed++;
        LOG_I(TAG, "%s: %s", s_list.names[i], fmt);
      } else {
        LOG_E(TAG, "%s: formato desconhecido ou save corrompido (%u bytes)", s_list.names[i],
              (unsigned)size);
      }
    }
    if (cache_ok) {
      saves_cache_record_encode(*entry, rec);
      cache_ok = sd_write(rec, SAVES_CACHE_RECORD_SIZE);
    }
    if (cb) cb(i + 1, s_list.count, user);
  }
  sd_close_write();
  free(rec);
  free(entry_p);
  if (!cache_ok) {
    LOG_E(TAG, "não gravou %s", SAVES_CACHE_PATH);
    sd_remove(SAVES_CACHE_PATH);
  }

  static uint8_t out[STATUS_CACHE_SIZE];
  status_cache_encode(s_list.sig, s_status, out);
  if (!sd_write_whole(STATUS_CACHE_PATH, out, sizeof(out))) {
    LOG_E(TAG, "não gravou %s", STATUS_CACHE_PATH);
  }

  LOG_I(TAG, "parse — %u/%u saves, %u vistos, %u capturados", rep.saves_parsed, rep.saves_found,
        s_status.count_seen(), s_status.count_caught());
  return rep;
}

const DexStatus &status_store_get() { return s_status; }

static StatusLoadReport s_last{};

StatusLoadReport status_store_load(bool force, StatusProgressCb cb, void *user) {
  s_last = load_impl(force, cb, user);
  save_store_invalidate();
  return s_last;
}

const StatusLoadReport &status_store_last_report() { return s_last; }
