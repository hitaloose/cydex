#include "sd.h"

#include <SD.h>
#include <cstring>

#include "board_pins.h"
#include "util/log.h"

static const char *TAG = "sd";
static bool s_present = false;
static File s_read;
static char s_read_path[64];
static size_t s_read_size = 0;

// O padrão do SD.begin é 4 MHz. 20 MHz é estável no CYD (VSPI dedicada).
static const uint32_t kSdFreqHz = 20000000;
// Arquivos abertos ao mesmo tempo, no máximo: o de sd_open_read (reaproveitado)
// + pasta e entrada do sd_list_files. O FATFS reserva ~4 KB POR arquivo num
// bloco contíguo só; o padrão (5) pede ~25 KB e, com o Wi-Fi, não cabe.
static const uint8_t kMaxFiles = 3;

bool sd_init() {
  sd_close_read();
  // Já montado (Sistema › Recarregar): não desmonta. Remontar com o Wi-Fi
  // ligado falha, porque o FATFS pede ~17 KB contíguos de novo.
  if (s_present && SD.cardType() != CARD_NONE) return true;
  SD.end();
  if (!SD.begin(PIN_SD_CS, SPI, kSdFreqHz, "/sd", kMaxFiles)) {
    LOG_E(TAG, "SD.begin falhou (sem cartão ou não é FAT32?)");
    s_present = false;
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    LOG_E(TAG, "nenhum cartão detectado");
    s_present = false;
    return false;
  }
  LOG_I(TAG, "ok — tipo=%d %lluMB", SD.cardType(), SD.cardSize() / (1024 * 1024));
  s_present = true;
  return true;
}

bool sd_is_present() { return s_present; }

void sd_list_files(const char *dir_path, const char *ext, SdListCallback cb, void *user) {
  if (!s_present) return;
  File dir = SD.open(dir_path);
  if (!dir || !dir.isDirectory()) {
    LOG_E(TAG, "%s não é diretório", dir_path);
    return;
  }
  size_t ext_len = strlen(ext);
  File e = dir.openNextFile();
  while (e) {
    if (!e.isDirectory()) {
      const char *name = e.name();
      size_t n = strlen(name);
      if (n >= ext_len && strcasecmp(name + n - ext_len, ext) == 0) {
        cb(name, e.size(), (uint32_t)e.getLastWrite(), user);
      }
    }
    e.close();
    e = dir.openNextFile();
  }
  dir.close();
}

bool sd_open_read(const char *path, size_t *out_size) {
  if (!s_present) return false;
  if (s_read && strcmp(s_read_path, path) == 0) {
    if (out_size) *out_size = s_read_size;
    return true;
  }
  sd_close_read();
  s_read = SD.open(path, FILE_READ);
  if (!s_read) {
    LOG_E(TAG, "falha ao abrir %s", path);
    return false;
  }
  strncpy(s_read_path, path, sizeof(s_read_path) - 1);
  s_read_path[sizeof(s_read_path) - 1] = '\0';
  s_read_size = s_read.size();
  if (out_size) *out_size = s_read_size;
  return true;
}

bool sd_read_at(size_t offset, uint8_t *out, size_t len) {
  if (!s_read) return false;
  if (!s_read.seek(offset)) return false;
  return s_read.read(out, len) == len;
}

void sd_close_read() {
  if (s_read) s_read.close();
  s_read_path[0] = '\0';
  s_read_size = 0;
}

bool sd_read_whole(const char *path, uint8_t *out, size_t len) {
  if (!s_present) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  bool ok = f.size() == len && f.read(out, len) == len;
  f.close();
  return ok;
}

bool sd_write_whole(const char *path, const uint8_t *data, size_t len) {
  if (!s_present) return false;
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    LOG_E(TAG, "falha ao gravar %s", path);
    return false;
  }
  bool ok = f.write(data, len) == len;
  f.close();
  return ok;
}
