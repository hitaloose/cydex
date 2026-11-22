#include "NameTable.h"

#include <cstring>

#include "utils/ByteUtils.h"

bool name_table_header_decode(const uint8_t *d, NameTableHeader *out) {
  if (memcmp(d, "CYNT", 4) != 0) return false;
  out->version = read_u16_le(d + 4);
  out->count = read_u16_le(d + 6);
  out->record_size = read_u16_le(d + 8);
  return out->version == NAME_TABLE_VERSION && out->record_size >= NAME_TABLE_NAME_SIZE;
}

void name_table_header_encode(uint16_t count, uint16_t record_size, uint8_t *out) {
  memset(out, 0, NAME_TABLE_HEADER_SIZE);
  memcpy(out, "CYNT", 4);
  out[4] = (uint8_t)NAME_TABLE_VERSION;
  out[6] = (uint8_t)count;
  out[7] = (uint8_t)(count >> 8);
  out[8] = (uint8_t)record_size;
  out[9] = (uint8_t)(record_size >> 8);
}

bool move_record_decode(const uint8_t *d, MoveInfo *out) {
  if (!memchr(d, 0, NAME_TABLE_NAME_SIZE)) return false;
  memcpy(out->name, d, NAME_TABLE_NAME_SIZE);
  out->type = d[24];
  out->pp = d[25];
  return true;
}

uint8_t move_pp_max(uint8_t base_pp, uint8_t pp_ups) {
  if (pp_ups > 3) pp_ups = 3;
  return (uint8_t)(base_pp + base_pp * pp_ups / 5);
}
