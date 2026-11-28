#include "NameRepo.h"

#include <cstring>

#include "hal/sd.h"
#include "paths.h"

namespace {

struct Table {
  const char *path;
  bool checked;
  bool ok;
  NameTableHeader h;
};

Table s_moves{MOVES_PATH, false, false, {}};
Table s_items_g3{ITEMS_G3_PATH, false, false, {}};
Table s_items_g4{ITEMS_G4_PATH, false, false, {}};

// Abre (reaproveitando, se já for o arquivo aberto) e confere o cabeçalho uma vez.
bool open_table(Table &t) {
  size_t size = 0;
  if (!sd_open_read(t.path, &size)) return false;
  if (!t.checked) {
    t.checked = true;
    uint8_t h[NAME_TABLE_HEADER_SIZE];
    t.ok = sd_read_at(0, h, sizeof(h)) && name_table_header_decode(h, &t.h) &&
           size >= NAME_TABLE_HEADER_SIZE + (size_t)t.h.count * t.h.record_size;
  }
  return t.ok;
}

}  // namespace

bool move_info(uint16_t id, MoveInfo *out) {
  if (id == 0 || !open_table(s_moves) || id > s_moves.h.count) return false;
  if (s_moves.h.record_size < MOVE_RECORD_SIZE) return false;
  uint8_t rec[MOVE_RECORD_SIZE];
  return sd_read_at(name_table_offset(s_moves.h, id), rec, sizeof(rec)) && move_record_decode(rec, out);
}

bool item_name(SaveGame game, uint16_t id, char *out, size_t size) {
  Table &t = game == SaveGame::HeartGoldSoulSilver ? s_items_g4 : s_items_g3;
  if (id == 0 || !open_table(t) || id > t.h.count || size == 0) return false;
  char name[NAME_TABLE_NAME_SIZE];
  if (!sd_read_at(name_table_offset(t.h, id), (uint8_t *)name, sizeof(name))) return false;
  name[sizeof(name) - 1] = 0;
  if (!name[0]) return false;
  strncpy(out, name, size - 1);
  out[size - 1] = 0;
  return true;
}
