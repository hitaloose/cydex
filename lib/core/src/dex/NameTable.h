#pragma once

#include <cstddef>
#include <cstdint>

// /cydex/moves.bin e /cydex/items_g3.bin / items_g4.bin: tabelas de nomes por índice, com
// registros fixos (seek em NAME_TABLE_HEADER_SIZE + (id-1) * registro).
// FONTE DA VERDADE do formato (tools/build_sd.py grava igual).
//
// Cabeçalho (16 B):
//   0  char[4] "CYNT"
//   4  u16     versão (NAME_TABLE_VERSION)
//   6  u16     quantidade de registros (ids 1..count)
//   8  u16     tamanho do registro
//   10 6 B     reservado
// Registro: char[24] nome UTF-8 terminado em NUL e depois os campos extras.
//   moves.bin    (28 B): 24 tipo (SpeciesType) · 25 PP base · 26 2 B reservado
//   items_g3.bin (24 B): só o nome, pelo índice do item no Gen3
//   items_g4.bin (24 B): idem, pelo índice do Gen4

constexpr uint16_t NAME_TABLE_VERSION = 1;
constexpr size_t NAME_TABLE_HEADER_SIZE = 16;
constexpr size_t NAME_TABLE_NAME_SIZE = 24;
constexpr size_t MOVE_RECORD_SIZE = 28;
constexpr size_t ITEM_RECORD_SIZE = 24;

struct NameTableHeader {
  uint16_t version;
  uint16_t count;
  uint16_t record_size;
};

bool name_table_header_decode(const uint8_t *data, NameTableHeader *out);
void name_table_header_encode(uint16_t count, uint16_t record_size, uint8_t *out);

inline size_t name_table_offset(const NameTableHeader &h, uint16_t id) {
  return NAME_TABLE_HEADER_SIZE + (size_t)(id - 1) * h.record_size;
}

struct MoveInfo {
  char name[NAME_TABLE_NAME_SIZE];
  uint8_t type;  // SpeciesType
  uint8_t pp;
};

// `data` = um registro de moves.bin. false se o nome não terminar em NUL.
bool move_record_decode(const uint8_t *data, MoveInfo *out);
// PP máximo com os PP-ups (0..3): base + base × PP-ups / 5 (Gen3).
uint8_t move_pp_max(uint8_t base_pp, uint8_t pp_ups);
