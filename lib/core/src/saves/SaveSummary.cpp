#include "SaveSummary.h"

#include <cstring>

#include "utils/ByteUtils.h"

// Layout (LE), conferido em save_summary_encode/decode:
//   0   u8   jogo (SaveGame)
//   1   char[24] nome
//   25  u8   gênero
//   26  u16  TID · 28 u16 SID
//   30  u16  horas · 32 u8 min · 33 u8 s
//   34  u32  dinheiro
//   38  u8   insígnias 0..7 (56: 8..15)
//   39  u16  vistos · 41 u16 capturados · 43 u16 total da dex
//   45  u8   times no Hall da Fama
//   46  u32  1ª vitória
//   50  u16  brilhantes · 52 u16 nas caixas · 54 u8 caixas · 55 u8 time
//   56  u8   insígnias 8..15 · 57 u8 total de insígnias (0 = 8, cache antigo)
//   64  6 × 28 B time: u16 espécie, u8 nível, u8 flags, char[24] apelido
//   232 50 × u32 hashes do Hall da Fama
//   432 493 × u8 onde está
//   925..927 reservado (0)

namespace {

struct W {
  uint8_t *p;
  void u8(size_t at, uint8_t v) { p[at] = v; }
  void u16(size_t at, uint16_t v) {
    p[at] = (uint8_t)v;
    p[at + 1] = (uint8_t)(v >> 8);
  }
  void u32(size_t at, uint32_t v) {
    for (int i = 0; i < 4; i++) p[at + i] = (uint8_t)(v >> (8 * i));
  }
  void str(size_t at, const char *s, size_t n) {
    strncpy((char *)p + at, s, n - 1);
    p[at + n - 1] = 0;
  }
};

void read_str(const uint8_t *src, char *dst, size_t n) {
  memcpy(dst, src, n);
  dst[n - 1] = 0;
}

constexpr size_t kParty = 64;
constexpr size_t kPartyRec = 28;
constexpr size_t kHof = kParty + SAVE_PARTY_MAX * kPartyRec;  // 232
constexpr size_t kWhere = kHof + SAVE_HOF_MAX * 4;              // 432
static_assert(kWhere == SAVE_SUMMARY_WHERE_AT, "offset do onde está");
static_assert(kHof == SAVE_SUMMARY_HOF_HASH_AT, "offset dos hashes");
static_assert(kParty == SAVE_SUMMARY_PARTY_AT && kPartyRec == SAVE_PARTY_BRIEF_SIZE, "offset do time");
static_assert(kWhere + SAVE_WHERE_SPECIES <= SAVE_SUMMARY_SIZE, "resumo não cabe");

}  // namespace

uint8_t SaveSummary::badge_count() const {
  uint8_t n = 0;
  for (int i = 0; i < SAVE_BADGES_MAX; i++) n += (badges >> i) & 1;
  return n;
}

uint8_t SaveSummary::where_of(uint16_t n) const {
  if (n < 1 || n > SAVE_WHERE_SPECIES) return SAVE_WHERE_NONE;
  return where[n - 1];
}

void save_summary_encode(const SaveSummary &s, uint8_t *out) {
  memset(out, 0, SAVE_SUMMARY_SIZE);
  W w{out};
  w.u8(0, (uint8_t)s.game);
  w.str(1, s.name, 24);
  w.u8(25, s.gender);
  w.u16(26, s.tid);
  w.u16(28, s.sid);
  w.u16(30, s.hours);
  w.u8(32, s.minutes);
  w.u8(33, s.seconds);
  w.u32(34, s.money);
  w.u8(38, (uint8_t)s.badges);
  w.u8(56, (uint8_t)(s.badges >> 8));
  w.u8(57, s.badge_total);
  w.u16(39, s.seen);
  w.u16(41, s.caught);
  w.u16(43, s.dex_total);
  w.u8(45, s.hof_count);
  w.u32(46, s.first_hof);
  w.u16(50, s.shiny_count);
  w.u16(52, s.box_count);
  w.u8(54, s.box_total);
  w.u8(55, s.party_count);
  for (int i = 0; i < SAVE_PARTY_MAX; i++) {
    size_t at = kParty + i * kPartyRec;
    w.u16(at, s.party[i].species);
    w.u8(at + 2, s.party[i].level);
    w.u8(at + 3, s.party[i].flags);
    w.str(at + 4, s.party[i].nick, 24);
  }
  for (int i = 0; i < SAVE_HOF_MAX; i++) w.u32(kHof + 4 * i, s.hof_hash[i]);
  memcpy(out + kWhere, s.where, SAVE_WHERE_SPECIES);
}

bool save_summary_decode(const uint8_t *d, SaveSummary *s) {
  memset(s, 0, sizeof(*s));
  if (d[0] > (uint8_t)SAVE_GAME_LAST) return false;
  s->game = (SaveGame)d[0];
  read_str(d + 1, s->name, 24);
  s->gender = d[25];
  s->tid = read_u16_le(d + 26);
  s->sid = read_u16_le(d + 28);
  s->hours = read_u16_le(d + 30);
  s->minutes = d[32];
  s->seconds = d[33];
  s->money = read_u32_le(d + 34);
  s->badges = (uint16_t)(d[38] | d[56] << 8);
  s->badge_total = d[57] ? d[57] : 8;
  s->seen = read_u16_le(d + 39);
  s->caught = read_u16_le(d + 41);
  s->dex_total = read_u16_le(d + 43);
  s->hof_count = d[45];
  s->first_hof = read_u32_le(d + 46);
  s->shiny_count = read_u16_le(d + 50);
  s->box_count = read_u16_le(d + 52);
  s->box_total = d[54];
  s->party_count = d[55];
  if (s->party_count > SAVE_PARTY_MAX || s->hof_count > SAVE_HOF_MAX) return false;
  for (int i = 0; i < SAVE_PARTY_MAX; i++) {
    const uint8_t *p = d + kParty + i * kPartyRec;
    s->party[i].species = read_u16_le(p);
    s->party[i].level = p[2];
    s->party[i].flags = p[3];
    read_str(p + 4, s->party[i].nick, 24);
  }
  for (int i = 0; i < SAVE_HOF_MAX; i++) s->hof_hash[i] = read_u32_le(d + kHof + 4 * i);
  memcpy(s->where, d + kWhere, SAVE_WHERE_SPECIES);
  return true;
}

bool save_party_brief_decode(const uint8_t *p, SavePartyBrief *out) {
  out->species = read_u16_le(p);
  out->level = p[2];
  out->flags = p[3];
  read_str(p + 4, out->nick, 24);
  return out->species != 0;
}

bool save_head_decode(const uint8_t *d, SaveHead *h) {
  memset(h, 0, sizeof(*h));
  if (d[0] > (uint8_t)SAVE_GAME_LAST || d[45] > SAVE_HOF_MAX) return false;
  h->game = (SaveGame)d[0];
  read_str(d + 1, h->name, 24);
  h->gender = d[25];
  h->tid = read_u16_le(d + 26);
  h->hours = read_u16_le(d + 30);
  h->minutes = d[32];
  h->badges = (uint16_t)(d[38] | d[56] << 8);
  h->badge_total = d[57] ? d[57] : 8;
  h->seen = read_u16_le(d + 39);
  h->caught = read_u16_le(d + 41);
  h->dex_total = read_u16_le(d + 43);
  h->hof_count = d[45];
  h->first_hof = read_u32_le(d + 46);
  h->box_count = read_u16_le(d + 52);
  h->box_total = d[54];
  h->party_count = d[55] <= SAVE_PARTY_MAX ? d[55] : 0;
  return true;
}
