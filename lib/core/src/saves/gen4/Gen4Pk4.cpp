#include "Gen4Pk4.h"

#include <cstring>

#include "Gen4Layout.h"
#include "Gen4Text.h"
#include "saves/PkBlockOrder.h"
#include "saves/gen3/Gen3Pk3.h"
#include "utils/ByteUtils.h"

namespace {

constexpr int kData = 0x08;
constexpr int kDataSize = 128;
constexpr int kBlock = 32;
constexpr int kExt = 0x88;
constexpr int kExtSize = GEN4_PK4_PARTY_SIZE - kExt;  // 100

void put_u16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}

// LCRNG do Gen4: XOR de cada u16 com a metade alta do próximo estado.
void crypt(uint8_t *data, int len, uint32_t seed) {
  for (int i = 0; i + 1 < len; i += 2) {
    seed = seed * 0x41C64E6Du + 0x6073u;
    put_u16(data + i, (uint16_t)(read_u16_le(data + i) ^ (seed >> 16)));
  }
}

uint16_t sum16(const uint8_t *data) {
  uint16_t s = 0;
  for (int i = 0; i < kDataSize; i += 2) s = (uint16_t)(s + read_u16_le(data + i));
  return s;
}

const uint8_t *block_pos(uint32_t pid) { return kPkBlockPos[((pid >> 13) & 31) % 24]; }

}  // namespace

void gen4_pk4_encrypt(uint8_t *raw, bool party) {
  uint32_t pid = read_u32_le(raw);
  uint8_t plain[kDataSize];
  memcpy(plain, raw + kData, kDataSize);
  uint16_t sum = sum16(plain);
  put_u16(raw + 0x06, sum);
  const uint8_t *pos = block_pos(pid);
  for (int i = 0; i < 4; i++) memcpy(raw + kData + pos[i] * kBlock, plain + i * kBlock, kBlock);
  crypt(raw + kData, kDataSize, sum);
  if (party) crypt(raw + kExt, kExtSize, pid);
}

bool gen4_pk4_decode(const uint8_t *raw, bool party, SaveMon *out) {
  memset(out, 0, sizeof(*out));
  uint32_t pid = read_u32_le(raw);
  uint16_t sum = read_u16_le(raw + 0x06);
  if (read_u16_le(raw + 0x04) & 0x04) return false;  // ovo ruim
  if (pid == 0 && sum == 0) return false;            // vaga vazia

  uint8_t enc[kDataSize];
  memcpy(enc, raw + kData, kDataSize);
  crypt(enc, kDataSize, sum);
  const uint8_t *pos = block_pos(pid);
  uint8_t d[kDataSize];  // em claro: A, B, C, D
  for (int i = 0; i < 4; i++) memcpy(d + i * kBlock, enc + pos[i] * kBlock, kBlock);
  if (sum16(d) != sum) return false;

  const uint8_t *a = d, *b = d + kBlock, *c = d + 2 * kBlock, *dd = d + 3 * kBlock;
  uint16_t species = read_u16_le(a);
  if (species == 0 || species > GEN4_DEX_SPECIES) return false;

  out->species = species;
  out->pid = pid;
  out->item = read_u16_le(a + 0x02);
  out->tid = read_u16_le(a + 0x04);
  out->sid = read_u16_le(a + 0x06);
  out->exp = read_u32_le(a + 0x08);
  // Ordem do jogo (PS, Atq, Def, Vel, AtE, DfE) -> ordem do species.bin.
  static const uint8_t kToDex[6] = {0, 1, 2, 5, 3, 4};
  uint32_t ivs = read_u32_le(b + 0x10);
  for (int i = 0; i < 6; i++) {
    out->evs[kToDex[i]] = a[0x10 + i];
    out->ivs[kToDex[i]] = (ivs >> (5 * i)) & 0x1F;
  }
  for (int i = 0; i < 4; i++) {
    out->moves[i] = read_u16_le(b + 2 * i);
    out->pp[i] = b[0x08 + i];
    out->pp_ups[i] = b[0x0C + i] & 3;
  }
  out->egg = (ivs >> 30) & 1;
  out->nature = (uint8_t)(pid % 25);
  out->shiny = gen3_is_shiny(out->tid, out->sid, pid);  // mesma fórmula
  gen4_text_decode(c, 11, out->nick, sizeof(out->nick));
  gen4_text_decode(dd, 8, out->ot, sizeof(out->ot));

  if (party) {
    uint8_t ext[kExtSize];
    memcpy(ext, raw + kExt, kExtSize);
    crypt(ext, kExtSize, pid);
    out->level = ext[0x04];
    out->hp = read_u16_le(ext + 0x06);
    for (int i = 0; i < 6; i++) out->stats[kToDex[i]] = read_u16_le(ext + 0x08 + 2 * i);
  }
  return true;
}
