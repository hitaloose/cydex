#include "Gen3Pk3.h"

#include <cstring>

#include "Gen3Species.h"
#include "Gen3TextDecoder.h"
#include "saves/PkBlockOrder.h"
#include "utils/ByteUtils.h"

namespace {

const auto &kBlockPos = kPkBlockPos;

constexpr int kData = 0x20;
constexpr int kDataSize = 48;
constexpr int kBlock = 12;

void xor_words(uint8_t *data, uint32_t key) {
  for (int i = 0; i < kDataSize; i += 4) {
    uint32_t w = read_u32_le(data + i) ^ key;
    data[i] = (uint8_t)w;
    data[i + 1] = (uint8_t)(w >> 8);
    data[i + 2] = (uint8_t)(w >> 16);
    data[i + 3] = (uint8_t)(w >> 24);
  }
}

uint16_t sum16(const uint8_t *data) {
  uint16_t s = 0;
  for (int i = 0; i < kDataSize; i += 2) s = (uint16_t)(s + read_u16_le(data + i));
  return s;
}

void put_u16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}

}  // namespace

bool gen3_is_shiny(uint16_t tid, uint16_t sid, uint32_t pid) {
  return (uint16_t)(tid ^ sid ^ (uint16_t)(pid >> 16) ^ (uint16_t)pid) < 8;
}

void gen3_pk3_encrypt(uint8_t *raw) {
  uint32_t pid = read_u32_le(raw);
  uint32_t key = pid ^ read_u32_le(raw + 4);
  uint8_t plain[kDataSize];
  memcpy(plain, raw + kData, kDataSize);
  put_u16(raw + 0x1C, sum16(plain));
  const uint8_t *pos = kBlockPos[pid % 24];
  for (int i = 0; i < 4; i++) memcpy(raw + kData + pos[i] * kBlock, plain + i * kBlock, kBlock);
  xor_words(raw + kData, key);
}

bool gen3_pk3_decode(const uint8_t *raw, bool party, SaveMon *out) {
  memset(out, 0, sizeof(*out));
  uint32_t pid = read_u32_le(raw);
  uint32_t otid = read_u32_le(raw + 4);
  uint8_t flags = raw[0x13];
  if (flags & 0x01) return false;  // ovo ruim
  if (!(flags & 0x02) && pid == 0 && otid == 0) return false;  // vaga vazia

  uint8_t enc[kDataSize];
  memcpy(enc, raw + kData, kDataSize);
  xor_words(enc, pid ^ otid);
  const uint8_t *pos = kBlockPos[pid % 24];
  uint8_t d[kDataSize];  // em claro: Growth, Attacks, EVs, Misc
  for (int i = 0; i < 4; i++) memcpy(d + i * kBlock, enc + pos[i] * kBlock, kBlock);
  if (sum16(d) != read_u16_le(raw + 0x1C)) return false;

  const uint8_t *g = d, *a = d + kBlock, *e = d + 2 * kBlock, *m = d + 3 * kBlock;
  uint16_t species = gen3_species_to_national(read_u16_le(g));
  if (species == 0) return false;

  out->species = species;
  out->pid = pid;
  out->tid = (uint16_t)otid;
  out->sid = (uint16_t)(otid >> 16);
  out->item = read_u16_le(g + 2);
  out->exp = read_u32_le(g + 4);
  for (int i = 0; i < 4; i++) {
    out->pp_ups[i] = (g[8] >> (2 * i)) & 3;
    out->moves[i] = read_u16_le(a + 2 * i);
    out->pp[i] = a[8 + i];
  }
  // Ordem do jogo (PS, Atq, Def, Vel, AtE, DfE) -> ordem do species.bin.
  static const uint8_t kToDex[6] = {0, 1, 2, 5, 3, 4};
  uint32_t ivs = read_u32_le(m + 4);
  for (int i = 0; i < 6; i++) {
    out->ivs[kToDex[i]] = (ivs >> (5 * i)) & 0x1F;
    out->evs[kToDex[i]] = e[i];
  }
  out->egg = (ivs >> 30) & 1;
  out->nature = (uint8_t)(pid % 25);
  out->shiny = gen3_is_shiny(out->tid, out->sid, pid);
  gen3_text_decode(raw + 0x08, 10, out->nick, sizeof(out->nick));
  gen3_text_decode(raw + 0x14, 7, out->ot, sizeof(out->ot));

  if (party) {
    out->level = raw[0x54];
    out->hp = read_u16_le(raw + 0x56);
    for (int i = 0; i < 6; i++) out->stats[kToDex[i]] = read_u16_le(raw + 0x58 + 2 * i);
  }
  return true;
}
