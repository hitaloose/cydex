#include "Gen4Save.h"

#include <cstring>

#include "Gen4Layout.h"
#include "Gen4Pk4.h"
#include "Gen4Text.h"
#include "saves/gen3/Gen3Pk3.h"
#include "utils/ByteUtils.h"

namespace {

const Gen4Layout *const kLayouts[] = {&GEN4_LAYOUT_HGSS};
constexpr uint8_t kLayoutCount = sizeof(kLayouts) / sizeof(kLayouts[0]);

struct State {
  uint32_t general;  // início do bloco geral da cópia escolhida
  uint32_t storage;  // idem, armazenamento
  uint32_t hof;      // idem, Hall da Fama (0 = sem)
  uint8_t layout;
};
static_assert(sizeof(State) <= sizeof(SaveHandle::state), "estado do Gen4 não cabe");
static_assert(GEN4_BOX_SLOTS * GEN4_PK4_STORED_SIZE <= SAVE_SCRATCH_SIZE, "caixa não cabe no rascunho");
static_assert(GEN4_BOX_SLOTS == SAVE_BOX_SLOTS, "vagas por caixa");
static_assert(GEN4_DEX_SPECIES <= SAVE_WHERE_SPECIES, "onde está não cobre o Gen4");

const State &st(const SaveHandle &h) { return *(const State *)h.state; }
const Gen4Layout &lay(const SaveHandle &h) { return *kLayouts[st(h).layout]; }

bool rd(const SaveReader &r, size_t at, void *out, size_t len) {
  return at + len <= r.size && r.read(at, (uint8_t *)out, len, r.user);
}

struct Footer {
  uint32_t counter, size, magic;
  uint16_t crc;
};

bool footer_at(const SaveReader &r, size_t block, uint32_t size, Footer *f) {
  uint8_t b[16];
  if (!rd(r, block + size - 16, b, 16)) return false;
  f->counter = read_u32_le(b);
  f->size = read_u32_le(b + 4);
  f->magic = read_u32_le(b + 8);
  f->crc = read_u16_le(b + 14);
  return f->magic == GEN4_FOOTER_MAGIC && f->size == size;
}

// CRC do bloco lido aos pedaços no rascunho.
bool crc_ok(const SaveReader &r, size_t block, uint32_t size, uint16_t want) {
  uint16_t crc = 0xFFFF;
  size_t left = size - 16;
  size_t at = block;
  while (left > 0) {
    size_t n = left < SAVE_SCRATCH_SIZE ? left : SAVE_SCRATCH_SIZE;
    if (!rd(r, at, r.scratch, n)) return false;
    crc = gen4_crc16(r.scratch, n, crc);
    at += n;
    left -= n;
  }
  return crc == want;
}

// Cópia mais nova com CRC válido do bloco em `off` (do começo da cópia).
// Só a mais nova é validada primeiro; se falhar, a outra.
bool pick_copy(const SaveReader &r, uint32_t off, uint32_t size, uint32_t *out) {
  Footer f[2];
  bool ok[2];
  for (int c = 0; c < 2; c++) ok[c] = footer_at(r, c * GEN4_COPY_OFFSET + off, size, &f[c]);
  int first = (ok[0] && ok[1]) ? (f[1].counter > f[0].counter ? 1 : 0) : (ok[1] ? 1 : 0);
  for (int k = 0; k < 2; k++) {
    int c = k == 0 ? first : 1 - first;
    if (!ok[c]) continue;
    uint32_t base = (uint32_t)(c * GEN4_COPY_OFFSET + off);
    if (crc_ok(r, base, size, f[c].crc)) {
      *out = base;
      return true;
    }
  }
  return false;
}

// Hall da Fama: a cópia com o maior contador entre as de mágico certo.
uint32_t pick_hof(const SaveReader &r, const Gen4Layout &L) {
  uint32_t best = 0, best_counter = 0;
  for (int c = 0; c < 2; c++) {
    size_t base = c * GEN4_COPY_OFFSET + L.hof_at;
    uint8_t b[12];
    if (!rd(r, base + L.hof_data, b, sizeof(b))) continue;
    if (read_u32_le(b) != GEN4_FOOTER_MAGIC || read_u32_le(b + 8) != L.hof_data) continue;
    uint32_t counter = read_u32_le(b + 4);
    if (best == 0 || counter > best_counter) {
      best = (uint32_t)base;
      best_counter = counter;
    }
  }
  return best;
}

int layout_of(const SaveReader &r) {
  if (r.size < GEN4_SAVE_SIZE || r.size >= 2 * GEN4_SAVE_SIZE) return -1;
  for (uint8_t i = 0; i < kLayoutCount; i++) {
    Footer f;
    for (int c = 0; c < 2; c++)
      if (footer_at(r, c * GEN4_COPY_OFFSET, kLayouts[i]->general_size, &f)) return i;
  }
  return -1;
}

bool detect(const SaveReader &r) { return layout_of(r) >= 0; }

bool open(SaveHandle &h) {
  if (!h.reader.scratch) return false;
  int li = layout_of(h.reader);
  if (li < 0) return false;
  const Gen4Layout &L = *kLayouts[li];
  State s{};
  s.layout = (uint8_t)li;
  if (!pick_copy(h.reader, 0, L.general_size, &s.general)) return false;
  if (!pick_copy(h.reader, L.storage_at, L.storage_size, &s.storage)) return false;
  s.hof = pick_hof(h.reader, L);
  memcpy(h.state, &s, sizeof(s));
  return true;
}

bool gen(const SaveHandle &h, uint32_t off, void *out, size_t len) {
  return rd(h.reader, st(h).general + off, out, len);
}

bool bit(const uint8_t *bits, uint16_t n) { return (bits[(n - 1) >> 3] >> ((n - 1) & 7)) & 1; }

// Capturados nos primeiros 0x40 B de `out`, vistos nos 0x40 seguintes.
bool read_dex(const SaveHandle &h, uint8_t *out) {
  const Gen4Layout &L = lay(h);
  return gen(h, L.dex + GEN4_DEX_CAUGHT, out, GEN4_DEX_BYTES) &&
         gen(h, L.dex + GEN4_DEX_SEEN, out + GEN4_DEX_BYTES, GEN4_DEX_BYTES);
}

bool merge_dex(const SaveReader &r, DexStatus &out) {
  SaveHandle h{};
  h.reader = r;
  if (!open(h)) return false;
  uint8_t dex[2 * GEN4_DEX_BYTES];
  if (!read_dex(h, dex)) return false;
  for (uint16_t n = 1; n <= GEN4_DEX_SPECIES; n++) {  // bits acima de 493 são lixo
    if (bit(dex, n)) out.mark_caught(n);
    else if (bit(dex + GEN4_DEX_BYTES, n)) out.mark_seen(n);
  }
  return true;
}

uint8_t party_count(const SaveHandle &h) {
  uint8_t n = 0;
  if (!gen(h, lay(h).party_count, &n, 1)) return 0;
  return n > 6 ? 6 : n;
}

bool read_party_mon(const SaveHandle &h, uint8_t i, SaveMon *m) {
  uint8_t raw[GEN4_PK4_PARTY_SIZE];
  if (!gen(h, lay(h).party + i * GEN4_PK4_PARTY_SIZE, raw, sizeof(raw))) return false;
  return gen4_pk4_decode(raw, true, m);
}

uint8_t read_party(const SaveHandle &h, SaveMon out[6]) {
  uint8_t count = party_count(h), n = 0;
  for (uint8_t i = 0; i < count; i++)
    if (read_party_mon(h, i, &out[n])) n++;
  for (uint8_t i = n; i < 6; i++) memset(&out[i], 0, sizeof(SaveMon));
  return n;
}

// Caixa crua (30 × 136 B) no rascunho.
bool read_box_raw(const SaveHandle &h, uint8_t box) {
  const Gen4Layout &L = lay(h);
  if (box >= L.box_count) return false;
  return rd(h.reader, st(h).storage + (size_t)box * L.box_stride, h.reader.scratch,
            GEN4_BOX_SLOTS * GEN4_PK4_STORED_SIZE);
}

bool read_box(const SaveHandle &h, uint8_t box, SaveMon out[SAVE_BOX_SLOTS], char *name, size_t name_size) {
  if (!read_box_raw(h, box)) return false;
  for (int i = 0; i < GEN4_BOX_SLOTS; i++)
    if (!gen4_pk4_decode(h.reader.scratch + i * GEN4_PK4_STORED_SIZE, false, &out[i]))
      memset(&out[i], 0, sizeof(SaveMon));
  if (name && name_size) {
    const Gen4Layout &L = lay(h);
    uint8_t n[0x28];
    if (rd(h.reader, st(h).storage + L.box_names + (size_t)box * L.box_name_stride, n, L.box_name_stride))
      gen4_text_decode(n, L.box_name_stride / 2, name, name_size);
    else
      name[0] = 0;
  }
  return true;
}

// Quantos times o Hall guarda e em que vaga está o time `team` (0 = o mais
// antigo). O jogo grava em anel de 30; o u32 depois dos 30 times é o total.
bool hof_slot(const SaveHandle &h, uint8_t team, uint8_t *slot) {
  if (!st(h).hof) return false;
  uint8_t b[4];
  if (!rd(h.reader, st(h).hof + GEN4_HOF_TOTAL_AT, b, 4)) return false;
  uint32_t total = read_u32_le(b);
  uint32_t count = total < GEN4_HOF_TEAMS ? total : GEN4_HOF_TEAMS;
  if (team >= count) return false;
  *slot = (uint8_t)(total <= GEN4_HOF_TEAMS ? team : (total + team) % GEN4_HOF_TEAMS);
  return true;
}

bool read_hof_raw(const SaveHandle &h, uint8_t team, uint8_t *raw) {
  uint8_t slot;
  if (!hof_slot(h, team, &slot)) return false;
  return rd(h.reader, st(h).hof + (size_t)slot * GEN4_HOF_TEAM_SIZE, raw, GEN4_HOF_TEAM_SIZE);
}

// Criatura do Hall (0x3C B): espécie u16, nível, forma, PID, TID, SID,
// apelido 11 × u16, treinador 8 × u16, 4 golpes.
void decode_hof_mon(const uint8_t *p, SaveMon *m) {
  memset(m, 0, sizeof(*m));
  uint16_t species = read_u16_le(p);
  if (species == 0 || species > GEN4_DEX_SPECIES) return;
  m->species = species;
  m->level = p[2];
  m->pid = read_u32_le(p + 4);
  m->tid = read_u16_le(p + 8);
  m->sid = read_u16_le(p + 10);
  m->nature = (uint8_t)(m->pid % 25);
  m->shiny = gen3_is_shiny(m->tid, m->sid, m->pid);
  gen4_text_decode(p + 0x0C, 11, m->nick, sizeof(m->nick));
  gen4_text_decode(p + 0x22, 8, m->ot, sizeof(m->ot));
  for (int i = 0; i < 4; i++) m->moves[i] = read_u16_le(p + 0x32 + 2 * i);
}

uint8_t read_hof_team(const SaveHandle &h, uint8_t team, SaveMon out[6]) {
  uint8_t raw[GEN4_HOF_TEAM_SIZE];
  uint8_t n = 0;
  if (read_hof_raw(h, team, raw)) {
    for (int i = 0; i < 6; i++) {
      decode_hof_mon(raw + i * GEN4_HOF_MON_SIZE, &out[n]);
      if (out[n].species) n++;
    }
  }
  for (uint8_t i = n; i < 6; i++) memset(&out[i], 0, sizeof(SaveMon));
  return n;
}

uint32_t fnv(uint32_t h, uint32_t v) {
  for (int i = 0; i < 4; i++) {
    h ^= (v >> (8 * i)) & 0xFF;
    h *= 16777619u;
  }
  return h;
}

void note_mon(SaveSummary &s, const SaveMon &m, uint8_t place) {
  if (m.shiny) s.shiny_count++;
  if (m.egg || m.species > SAVE_WHERE_SPECIES) return;
  uint8_t &w = s.where[m.species - 1];
  if (place == SAVE_WHERE_PARTY || w == SAVE_WHERE_NONE) w = place;
}

bool summarize(const SaveHandle &h, SaveSummary &s) {
  memset(&s, 0, sizeof(s));
  const Gen4Layout &L = lay(h);
  s.game = L.game;

  uint8_t name[16];
  if (gen(h, L.trainer_name, name, sizeof(name))) gen4_text_decode(name, 8, s.name, sizeof(s.name));
  uint8_t b[4];
  if (gen(h, L.tid, b, 2)) s.tid = read_u16_le(b);
  if (gen(h, L.sid, b, 2)) s.sid = read_u16_le(b);
  if (gen(h, L.money, b, 4)) s.money = read_u32_le(b);
  if (gen(h, L.gender, b, 1)) s.gender = b[0] ? 1 : 0;
  if (gen(h, L.badges, b, 1)) s.badges = b[0];
  if (L.badges2 && gen(h, L.badges2, b, 1)) s.badges |= (uint16_t)(b[0] << 8);
  s.badge_total = L.badge_total;
  if (gen(h, L.played_hours, b, 2)) s.hours = read_u16_le(b);
  if (gen(h, L.played_minutes, b, 1)) s.minutes = b[0];
  if (gen(h, L.played_seconds, b, 1)) s.seconds = b[0];

  uint8_t dex[2 * GEN4_DEX_BYTES];
  s.dex_total = GEN4_DEX_SPECIES;
  if (read_dex(h, dex)) {
    for (uint16_t n = 1; n <= GEN4_DEX_SPECIES; n++) {
      bool caught = bit(dex, n);
      if (caught) s.caught++;
      if (caught || bit(dex + GEN4_DEX_BYTES, n)) s.seen++;
    }
  }

  // Uma criatura por vez (a RAM estática não comporta uma caixa decodificada).
  SaveMon m;
  uint8_t count = party_count(h);
  for (uint8_t i = 0; i < count; i++) {
    if (!read_party_mon(h, i, &m)) continue;
    SavePartyBrief &p = s.party[s.party_count++];
    p.species = m.species;
    p.level = m.level;
    p.flags = (m.shiny ? SAVE_MON_SHINY : 0) | (m.egg ? SAVE_MON_EGG : 0);
    strncpy(p.nick, m.nick, sizeof(p.nick) - 1);
    note_mon(s, m, SAVE_WHERE_PARTY);
  }

  s.box_total = L.box_count;
  for (uint8_t bx = 0; bx < L.box_count; bx++) {
    if (!read_box_raw(h, bx)) break;
    for (int i = 0; i < GEN4_BOX_SLOTS; i++) {
      if (!gen4_pk4_decode(h.reader.scratch + i * GEN4_PK4_STORED_SIZE, false, &m)) continue;
      s.box_count++;
      note_mon(s, m, (uint8_t)(bx + 1));
    }
  }

  // O Gen4 guarda a data de cada vitória, não o tempo de jogo: first_hof = 0.
  uint8_t raw[GEN4_HOF_TEAM_SIZE];
  for (uint8_t t = 0; t < GEN4_HOF_TEAMS && s.hof_count < SAVE_HOF_MAX; t++) {
    if (!read_hof_raw(h, t, raw)) break;
    uint16_t sp = read_u16_le(raw);
    if (sp == 0 || sp > GEN4_DEX_SPECIES) break;
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 6; i++) {
      const uint8_t *p = raw + i * GEN4_HOF_MON_SIZE;
      hash = fnv(hash, read_u32_le(p + 8));  // TID + SID
      hash = fnv(hash, read_u32_le(p + 4));  // PID
    }
    s.hof_hash[s.hof_count++] = hash;
  }
  return true;
}

}  // namespace

uint16_t gen4_crc16(const uint8_t *data, size_t len, uint16_t crc) {
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)(data[i] << 8);
    for (int k = 0; k < 8; k++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

const SaveFormat GEN4_SAVE_FORMAT = {
    "gen4", detect, merge_dex, open, summarize, read_party, read_box, read_hof_team,
};
