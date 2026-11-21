#include "StatusStore.h"

#include <cstdio>
#include <cstring>

#include "dex/StatusCache.h"
#include "hal/sd.h"
#include "paths.h"
#include "saves/SaveFormat.h"
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
        status_cache_decode(cache, sizeof(cache), s_list.sig, &s_status)) {
      rep.from_cache = true;
      rep.saves_parsed = s_list.count;
      if (cb) cb(s_list.count, s_list.count, user);
      LOG_I(TAG, "cache ok — %u saves, %u vistos, %u capturados", s_list.count,
            s_status.count_seen(), s_status.count_caught());
      return rep;
    }
  }

  for (uint8_t i = 0; i < s_list.count; i++) {
    char path[kMaxName + 16];
    snprintf(path, sizeof(path), "%s/%s", SAVES_DIR, s_list.names[i]);
    size_t size = 0;
    if (sd_open_read(path, &size)) {
      SaveReader r{size, sd_reader, nullptr};
      const char *fmt = save_merge_dex(r, s_status);
      sd_close_read();
      if (fmt) {
        rep.saves_parsed++;
        LOG_I(TAG, "%s: %s", s_list.names[i], fmt);
      } else {
        LOG_E(TAG, "%s: formato desconhecido ou save corrompido (%u bytes)", s_list.names[i],
              (unsigned)size);
      }
    }
    if (cb) cb(i + 1, s_list.count, user);
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
  return s_last;
}

const StatusLoadReport &status_store_last_report() { return s_last; }
