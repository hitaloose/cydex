#include <unity.h>

#include <cstring>
#include <vector>

#include "dex/NameTable.h"
#include "dex/SpeciesRecord.h"
#include "logic/MonStats.h"
#include "saves/SaveFormat.h"
#include "saves/SaveSummary.h"
#include "saves/SavesCache.h"
#include "saves/gen3/Gen3Pk3.h"
#include "saves/gen3/Gen3Sections.h"
#include "saves/gen3/Gen3Species.h"
#include "utils/ByteUtils.h"

static uint8_t g_scratch[SAVE_SCRATCH_SIZE];

void setUp() {}
void tearDown() {}

// ---------- helpers ----------

static void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

// Texto Gen3: A=0xBB.., a=0xD5.., terminador 0xFF.
static void put_text(uint8_t *p, const char *s, int len) {
  memset(p, 0xFF, len);
  for (int i = 0; s[i] && i < len; i++) {
    char c = s[i];
    p[i] = (c >= 'A' && c <= 'Z') ? (uint8_t)(0xBB + c - 'A') : (uint8_t)(0xD5 + c - 'a');
  }
}

struct MonSpec {
  uint32_t pid;
  uint16_t tid, sid;
  uint16_t internal_species;
  const char *nick;
  uint32_t exp;
  uint16_t item;
  uint16_t moves[4];
  uint8_t pp[4];
  uint8_t pp_ups;
  uint32_t ivs;  // bits do jogo (PS, Atq, Def, Vel, AtE, DfE), bit 30 ovo
  uint8_t evs[6];
};

// PK3 de 80 B (ou 100 com o nível/status do time) já cifrado.
static void make_pk3(uint8_t *raw, const MonSpec &m, bool party = false, uint8_t level = 0) {
  memset(raw, 0, party ? GEN3_PK3_PARTY_SIZE : GEN3_PK3_STORED_SIZE);
  put32(raw, m.pid);
  put16(raw + 4, m.tid);
  put16(raw + 6, m.sid);
  put_text(raw + 8, m.nick, 10);
  raw[0x12] = 2;
  raw[0x13] = 0x02;
  put_text(raw + 0x14, "Ana", 7);
  uint8_t *g = raw + 0x20, *a = g + 12, *e = g + 24, *mi = g + 36;
  put16(g, m.internal_species);
  put16(g + 2, m.item);
  put32(g + 4, m.exp);
  g[8] = m.pp_ups;
  for (int i = 0; i < 4; i++) {
    put16(a + 2 * i, m.moves[i]);
    a[8 + i] = m.pp[i];
  }
  memcpy(e, m.evs, 6);
  put32(mi + 4, m.ivs);
  gen3_pk3_encrypt(raw);
  if (party) {
    raw[0x54] = level;
    put16(raw + 0x56, 77);
    for (int i = 0; i < 6; i++) put16(raw + 0x58 + 2 * i, (uint16_t)(100 + i));
  }
}

static MonSpec basic(uint32_t pid, uint16_t internal) {
  MonSpec m{};
  m.pid = pid;
  m.tid = 4821;
  m.sid = 1234;
  m.internal_species = internal;
  m.nick = "Lama";
  m.exp = 1000;
  return m;
}

// ---------- espécies ----------

void test_species_conversion() {
  TEST_ASSERT_EQUAL_UINT16(1, gen3_species_to_national(1));
  TEST_ASSERT_EQUAL_UINT16(251, gen3_species_to_national(251));
  TEST_ASSERT_EQUAL_UINT16(0, gen3_species_to_national(252));
  TEST_ASSERT_EQUAL_UINT16(0, gen3_species_to_national(276));
  TEST_ASSERT_EQUAL_UINT16(252, gen3_species_to_national(277));  // Treecko
  TEST_ASSERT_EQUAL_UINT16(260, gen3_species_to_national(285));  // Swampert
  TEST_ASSERT_EQUAL_UINT16(358, gen3_species_to_national(411));  // Chimecho
  TEST_ASSERT_EQUAL_UINT16(0, gen3_species_to_national(412));
  TEST_ASSERT_EQUAL_UINT16(0, gen3_species_to_national(0));
}

void test_species_conversion_is_a_bijection_on_hoenn() {
  bool seen[387] = {false};
  for (uint16_t i = 277; i <= 411; i++) {
    uint16_t n = gen3_species_to_national(i);
    TEST_ASSERT_TRUE(n >= 252 && n <= 386);
    TEST_ASSERT_FALSE(seen[n]);
    seen[n] = true;
  }
}

// ---------- PK3 ----------

void test_pk3_roundtrip_every_block_order() {
  for (uint32_t k = 0; k < 24; k++) {
    MonSpec m = basic(0x1000 * 24 + k, 285);
    m.item = 200;
    m.moves[0] = 57;
    m.moves[1] = 89;
    m.moves[2] = 58;
    m.moves[3] = 182;
    m.pp[0] = 15;
    m.pp[1] = 8;
    m.pp[2] = 10;
    m.pp[3] = 10;
    m.pp_ups = 0b11100100;  // 0, 1, 2, 3
    m.ivs = (31u << 0) | (30u << 5) | (29u << 10) | (28u << 15) | (27u << 20) | (26u << 25);
    uint8_t ev[6] = {1, 2, 3, 4, 5, 6};
    memcpy(m.evs, ev, 6);
    uint8_t raw[GEN3_PK3_STORED_SIZE];
    make_pk3(raw, m);
    SaveMon out;
    TEST_ASSERT_TRUE_MESSAGE(gen3_pk3_decode(raw, false, &out), "decode");
    TEST_ASSERT_EQUAL_UINT16(260, out.species);
    TEST_ASSERT_EQUAL_STRING("Lama", out.nick);
    TEST_ASSERT_EQUAL_STRING("Ana", out.ot);
    TEST_ASSERT_EQUAL_UINT16(200, out.item);
    TEST_ASSERT_EQUAL_UINT32(1000, out.exp);
    TEST_ASSERT_EQUAL_UINT16(89, out.moves[1]);
    TEST_ASSERT_EQUAL_UINT8(8, out.pp[1]);
    TEST_ASSERT_EQUAL_UINT8(0, out.pp_ups[0]);
    TEST_ASSERT_EQUAL_UINT8(3, out.pp_ups[3]);
    // Ordem do species.bin: PS, Atq, Def, AtE, DfE, Vel.
    TEST_ASSERT_EQUAL_UINT8(31, out.ivs[0]);
    TEST_ASSERT_EQUAL_UINT8(30, out.ivs[1]);
    TEST_ASSERT_EQUAL_UINT8(29, out.ivs[2]);
    TEST_ASSERT_EQUAL_UINT8(27, out.ivs[3]);
    TEST_ASSERT_EQUAL_UINT8(26, out.ivs[4]);
    TEST_ASSERT_EQUAL_UINT8(28, out.ivs[5]);
    TEST_ASSERT_EQUAL_UINT8(4, out.evs[5]);  // Vel
    TEST_ASSERT_EQUAL_UINT8(5, out.evs[3]);  // AtE
    TEST_ASSERT_EQUAL_UINT8((0x1000 * 24 + k) % 25, out.nature);
    TEST_ASSERT_EQUAL_UINT8(0, out.level);  // caixa: sem nível
  }
}

void test_pk3_party_fields() {
  uint8_t raw[GEN3_PK3_PARTY_SIZE];
  make_pk3(raw, basic(77, 25), true, 42);
  SaveMon out;
  TEST_ASSERT_TRUE(gen3_pk3_decode(raw, true, &out));
  TEST_ASSERT_EQUAL_UINT16(25, out.species);
  TEST_ASSERT_EQUAL_UINT8(42, out.level);
  TEST_ASSERT_EQUAL_UINT16(77, out.hp);
  // Jogo: PS 100, Atq 101, Def 102, Vel 103, AtE 104, DfE 105.
  TEST_ASSERT_EQUAL_UINT16(100, out.stats[0]);
  TEST_ASSERT_EQUAL_UINT16(103, out.stats[5]);
  TEST_ASSERT_EQUAL_UINT16(104, out.stats[3]);
}

void test_pk3_rejects_empty_and_corrupt() {
  uint8_t raw[GEN3_PK3_STORED_SIZE] = {0};
  SaveMon out;
  TEST_ASSERT_FALSE(gen3_pk3_decode(raw, false, &out));
  make_pk3(raw, basic(123456, 25));
  raw[0x25] ^= 0x40;  // estraga os dados cifrados
  TEST_ASSERT_FALSE(gen3_pk3_decode(raw, false, &out));
  make_pk3(raw, basic(123456, 25));
  raw[0x13] |= 0x01;  // ovo ruim
  TEST_ASSERT_FALSE(gen3_pk3_decode(raw, false, &out));
}

void test_pk3_egg_flag() {
  MonSpec m = basic(5, 25);
  m.ivs = 1u << 30;
  uint8_t raw[GEN3_PK3_STORED_SIZE];
  make_pk3(raw, m);
  SaveMon out;
  TEST_ASSERT_TRUE(gen3_pk3_decode(raw, false, &out));
  TEST_ASSERT_TRUE(out.egg);
}

void test_shiny_formula() {
  // TID ^ SID ^ PIDhi ^ PIDlo < 8
  TEST_ASSERT_TRUE(gen3_is_shiny(0x1234, 0x5678, ((uint32_t)0x1234 << 16) | 0x5678));
  TEST_ASSERT_TRUE(gen3_is_shiny(0x1234, 0x5678, ((uint32_t)0x1234 << 16) | 0x567F));
  TEST_ASSERT_FALSE(gen3_is_shiny(0x1234, 0x5678, ((uint32_t)0x1234 << 16) | 0x5670));
}

// ---------- MonStats ----------

void test_exp_curves_at_100() {
  TEST_ASSERT_EQUAL_UINT32(800000, exp_for_level(GrowthRate::Fast, 100));
  TEST_ASSERT_EQUAL_UINT32(1000000, exp_for_level(GrowthRate::Medium, 100));
  TEST_ASSERT_EQUAL_UINT32(1059860, exp_for_level(GrowthRate::MediumSlow, 100));
  TEST_ASSERT_EQUAL_UINT32(1250000, exp_for_level(GrowthRate::Slow, 100));
  TEST_ASSERT_EQUAL_UINT32(600000, exp_for_level(GrowthRate::Erratic, 100));
  TEST_ASSERT_EQUAL_UINT32(1640000, exp_for_level(GrowthRate::Fluctuating, 100));
  TEST_ASSERT_EQUAL_UINT32(0, exp_for_level(GrowthRate::MediumSlow, 1));
  TEST_ASSERT_EQUAL_UINT32(9, exp_for_level(GrowthRate::MediumSlow, 2));
}

void test_level_from_exp() {
  TEST_ASSERT_EQUAL_UINT8(1, level_from_exp(GrowthRate::Medium, 0));
  TEST_ASSERT_EQUAL_UINT8(10, level_from_exp(GrowthRate::Medium, 1000));
  TEST_ASSERT_EQUAL_UINT8(9, level_from_exp(GrowthRate::Medium, 999));
  TEST_ASSERT_EQUAL_UINT8(100, level_from_exp(GrowthRate::Medium, 5000000));
  TEST_ASSERT_EQUAL_UINT8(0, level_from_exp(GrowthRate::Unknown, 1000));
  for (uint8_t l = 1; l <= 100; l++) {
    TEST_ASSERT_EQUAL_UINT8(l, level_from_exp(GrowthRate::Erratic, exp_for_level(GrowthRate::Erratic, l)));
    TEST_ASSERT_EQUAL_UINT8(l, level_from_exp(GrowthRate::Fluctuating, exp_for_level(GrowthRate::Fluctuating, l)));
    TEST_ASSERT_EQUAL_UINT8(l, level_from_exp(GrowthRate::MediumSlow, exp_for_level(GrowthRate::MediumSlow, l)));
  }
}

void test_stat_formula_bulbapedia_example() {
  // Garchomp Nv 78 Adamant (3): PS base 108 IV 24 EV 74 = 289; Atq base 130
  // IV 12 EV 190 = 278; Def (neutra) base 95 IV 30 EV 91 = 193.
  TEST_ASSERT_EQUAL_UINT16(289, mon_stat(0, 108, 24, 74, 78, 3));
  TEST_ASSERT_EQUAL_UINT16(278, mon_stat(1, 130, 12, 190, 78, 3));
  TEST_ASSERT_EQUAL_UINT16(193, mon_stat(2, 95, 30, 91, 78, 3));
  uint8_t up, down;
  nature_effect(3, &up, &down);
  TEST_ASSERT_EQUAL_UINT8(1, up);
  TEST_ASSERT_EQUAL_UINT8(3, down);
  nature_effect(0, &up, &down);  // Hardy: neutra
  TEST_ASSERT_EQUAL_UINT8(up, down);
  nature_effect(24, &up, &down);  // Quirky: neutra
  TEST_ASSERT_EQUAL_UINT8(up, down);
  nature_effect(10, &up, &down);  // Timid: +Vel −Atq
  TEST_ASSERT_EQUAL_UINT8(5, up);
  TEST_ASSERT_EQUAL_UINT8(1, down);
}

void test_fill_stats_for_box_mon() {
  SaveMon m{};
  m.species = 292;  // Shedinja: PS sempre 1
  m.exp = 1000;
  uint8_t base[6] = {1, 90, 45, 30, 30, 40};
  mon_fill_stats(m, base, GrowthRate::Medium);
  TEST_ASSERT_EQUAL_UINT8(10, m.level);
  TEST_ASSERT_EQUAL_UINT16(1, m.stats[0]);
  TEST_ASSERT_EQUAL_UINT16(1, m.hp);
  SaveMon u{};
  u.species = 25;
  u.exp = 1000;
  mon_fill_stats(u, base, GrowthRate::Unknown);
  TEST_ASSERT_EQUAL_UINT8(0, u.level);
  TEST_ASSERT_EQUAL_UINT16(0, u.stats[1]);
}

// ---------- save inteiro (Emerald sintético) ----------

struct Built {
  std::vector<uint8_t> data;
};

static void footer(uint8_t *sec, uint16_t id, uint32_t counter) {
  put16(sec + GEN3_SECTOR_FOOTER_ID_OFFSET, id);
  put16(sec + GEN3_SECTOR_FOOTER_CHECKSUM_OFFSET, gen3_checksum32(sec, GEN3_SECTOR_DATA_SIZE));
  put32(sec + GEN3_SECTOR_FOOTER_COUNTER_OFFSET, counter);
}

// Slot 0 com as seções fora de ordem (física i guarda a lógica (i+3)%14),
// pra conferir que o leitor segue os IDs.
static std::vector<uint8_t> build(const std::vector<uint8_t> &small, const std::vector<uint8_t> &large,
                                  const std::vector<uint8_t> &storage, const std::vector<uint8_t> &hof) {
  std::vector<uint8_t> d(GEN3_SAVE_FILE_SIZE, 0);
  for (int i = 0; i < GEN3_SECTIONS_PER_SLOT; i++) {
    int id = (i + 3) % GEN3_SECTIONS_PER_SLOT;
    uint8_t *sec = d.data() + i * GEN3_SECTOR_SIZE;
    const uint8_t *src = nullptr;
    if (id == 0) src = small.data();
    else if (id <= 4) src = large.data() + (id - 1) * GEN3_SECTOR_DATA_SIZE;
    else src = storage.data() + (id - 5) * GEN3_SECTOR_DATA_SIZE;
    memcpy(sec, src, GEN3_SECTOR_DATA_SIZE);
    footer(sec, (uint16_t)id, 5);
  }
  for (int s = 0; s < 2; s++) {
    uint8_t *sec = d.data() + (s ? GEN3_HOF_SECTOR_B : GEN3_HOF_SECTOR_A);
    memcpy(sec, hof.data() + s * GEN3_SECTOR_DATA_SIZE, GEN3_SECTOR_DATA_SIZE);
    put16(sec + GEN3_EXTRA_CHECKSUM_OFFSET, gen3_checksum32(sec, GEN3_SECTOR_DATA_SIZE));
  }
  return d;
}

static bool mem_read(size_t off, uint8_t *out, size_t len, void *user) {
  const std::vector<uint8_t> *v = (const std::vector<uint8_t> *)user;
  if (off + len > v->size()) return false;
  memcpy(out, v->data() + off, len);
  return true;
}

static const uint32_t kKey = 0xA5A55A5A;

static std::vector<uint8_t> emerald_save() {
  std::vector<uint8_t> small(GEN3_SECTOR_DATA_SIZE, 0), large(4 * GEN3_SECTOR_DATA_SIZE, 0),
      storage(9 * GEN3_SECTOR_DATA_SIZE, 0), hof(2 * GEN3_SECTOR_DATA_SIZE, 0);
  put_text(small.data(), "Ana", 7);
  small[GEN3_SMALL_GENDER_OFFSET] = 1;
  put16(&small[GEN3_SMALL_TID_OFFSET], 4821);
  put16(&small[GEN3_SMALL_SID_OFFSET], 1234);
  put16(&small[GEN3_SMALL_PLAYED_HOURS_OFFSET], 42);
  small[GEN3_SMALL_PLAYED_MINUTES_OFFSET] = 18;
  put32(&small[GEN3_SMALL_SECURITY_KEY_EMERALD], kKey);  // também é a sonda de versão
  small[0x900] = 1;                                      // área só do Emerald
  // dex: 1..3 capturados, 4 visto
  small[GEN3_SMALL_POKEDEX_CAUGHT_OFFSET] = 0b0111;
  small[GEN3_SMALL_POKEDEX_SEEN_OFFSET] = 0b1111;

  const Gen3LargeLayout &L = GEN3_LARGE_EMERALD;
  put32(&large[L.money], 128450 ^ kKey);
  put32(&large[L.records + 4 * GEN3_RECORD_FIRST_HOF_PLAY_TIME], ((24u << 16) | (10u << 8) | 5u) ^ kKey);
  for (int i = 0; i < 5; i++) {  // 5 insígnias
    int flag = GEN3_BADGE_LOCATION_EMERALD.badge_flag_start + i;
    large[GEN3_BADGE_LOCATION_EMERALD.event_flag_base + (flag >> 3)] |= (uint8_t)(1 << (flag & 7));
  }
  large[L.party_count] = 2;
  make_pk3(&large[L.party], basic(1000, 285), true, 58);  // Swampert
  MonSpec sh = basic(0, 25);                              // Pikachu brilhante
  sh.pid = ((uint32_t)(4821 ^ 1234) << 16) | 0;
  make_pk3(&large[L.party + GEN3_PK3_PARTY_SIZE], sh, true, 30);

  // Caixa 1 (índice 0): Bulbasaur na vaga 0; caixa 3 (índice 2): Pikachu e
  // Treecko. Nome da caixa 3: "Agua".
  make_pk3(&storage[GEN3_STORAGE_BOXES_OFFSET], basic(7, 1));
  size_t b2 = GEN3_STORAGE_BOXES_OFFSET + 2 * 30 * GEN3_PK3_STORED_SIZE;
  make_pk3(&storage[b2 + 5 * GEN3_PK3_STORED_SIZE], basic(8, 25));
  make_pk3(&storage[b2 + 29 * GEN3_PK3_STORED_SIZE], basic(9, 277));
  put_text(&storage[GEN3_STORAGE_BOX_NAMES_OFFSET + 2 * GEN3_BOX_NAME_LENGTH], "Agua", 9);

  // Hall da Fama: 34 times (o 34º, índice 33, atravessa a divisa dos setores).
  for (int t = 0; t < 34; t++) {
    for (int k = 0; k < 6; k++) {
      uint8_t *p = &hof[t * GEN3_HOF_TEAM_SIZE + k * GEN3_HOF_MON_SIZE];
      put16(p, 4821);
      put16(p + 2, 1234);
      put32(p + 4, (uint32_t)(t * 10 + k + 1));
      put16(p + 8, (uint16_t)((k == 0 ? 285 : 25 + k) | ((50 + t) << 9)));
      put_text(p + 10, "Hof", 10);
    }
  }
  return build(small, large, storage, hof);
}

void test_save_summary_from_emerald() {
  std::vector<uint8_t> d = emerald_save();
  SaveReader r{d.size(), mem_read, &d, g_scratch};
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  SaveSummary s;
  TEST_ASSERT_TRUE(h.format->summarize(h, s));
  TEST_ASSERT_EQUAL((int)SaveGame::Emerald, (int)s.game);
  TEST_ASSERT_EQUAL_STRING("Ana", s.name);
  TEST_ASSERT_EQUAL_UINT8(1, s.gender);
  TEST_ASSERT_EQUAL_UINT16(4821, s.tid);
  TEST_ASSERT_EQUAL_UINT16(42, s.hours);
  TEST_ASSERT_EQUAL_UINT8(18, s.minutes);
  TEST_ASSERT_EQUAL_UINT32(128450, s.money);
  TEST_ASSERT_EQUAL_UINT8(5, s.badge_count());
  TEST_ASSERT_EQUAL_UINT16(4, s.seen);
  TEST_ASSERT_EQUAL_UINT16(3, s.caught);
  TEST_ASSERT_EQUAL_UINT16(386, s.dex_total);
  TEST_ASSERT_EQUAL_UINT8(2, s.party_count);
  TEST_ASSERT_EQUAL_UINT16(260, s.party[0].species);
  TEST_ASSERT_EQUAL_UINT8(58, s.party[0].level);
  TEST_ASSERT_EQUAL_STRING("Lama", s.party[0].nick);
  TEST_ASSERT_TRUE(s.party[1].flags & SAVE_MON_SHINY);
  TEST_ASSERT_EQUAL_UINT16(1, s.shiny_count);
  TEST_ASSERT_EQUAL_UINT16(3, s.box_count);
  TEST_ASSERT_EQUAL_UINT8(14, s.box_total);
  TEST_ASSERT_EQUAL_UINT8(SAVE_WHERE_PARTY, s.where_of(260));
  TEST_ASSERT_EQUAL_UINT8(SAVE_WHERE_PARTY, s.where_of(25));  // o time vence a caixa
  TEST_ASSERT_EQUAL_UINT8(1, s.where_of(1));
  TEST_ASSERT_EQUAL_UINT8(3, s.where_of(252));
  TEST_ASSERT_EQUAL_UINT8(SAVE_WHERE_NONE, s.where_of(4));
  TEST_ASSERT_EQUAL_UINT8(34, s.hof_count);
  TEST_ASSERT_EQUAL_UINT32((24u << 16) | (10u << 8) | 5u, s.first_hof);
  TEST_ASSERT_NOT_EQUAL(s.hof_hash[0], s.hof_hash[1]);
}

void test_save_box_and_hof_reads() {
  std::vector<uint8_t> d = emerald_save();
  SaveReader r{d.size(), mem_read, &d, g_scratch};
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  SaveMon box[SAVE_BOX_SLOTS];
  char name[24];
  TEST_ASSERT_TRUE(h.format->read_box(h, 2, box, name, sizeof(name)));
  TEST_ASSERT_EQUAL_STRING("Agua", name);
  TEST_ASSERT_EQUAL_UINT16(25, box[5].species);
  TEST_ASSERT_EQUAL_UINT16(252, box[29].species);
  TEST_ASSERT_EQUAL_UINT16(0, box[0].species);
  TEST_ASSERT_FALSE(h.format->read_box(h, 14, box, name, sizeof(name)));

  SaveMon team[6];
  TEST_ASSERT_EQUAL_UINT8(6, h.format->read_hof_team(h, 0, team));
  TEST_ASSERT_EQUAL_UINT16(260, team[0].species);
  TEST_ASSERT_EQUAL_UINT8(50, team[0].level);
  TEST_ASSERT_EQUAL_STRING("Hof", team[0].nick);
  // O 34º time atravessa os dois setores.
  TEST_ASSERT_EQUAL_UINT8(6, h.format->read_hof_team(h, 33, team));
  TEST_ASSERT_EQUAL_UINT8(83, team[5].level);
  TEST_ASSERT_EQUAL_UINT32(336, team[5].pid);
  TEST_ASSERT_EQUAL_UINT8(0, h.format->read_hof_team(h, 34, team));

  SaveMon party[6];
  TEST_ASSERT_EQUAL_UINT8(2, h.format->read_party(h, party));
  TEST_ASSERT_EQUAL_UINT16(77, party[0].hp);
}

void test_hof_with_bad_checksum_is_ignored() {
  std::vector<uint8_t> d = emerald_save();
  d[GEN3_HOF_SECTOR_A + 3] ^= 0xFF;  // sem refazer o checksum
  SaveReader r{d.size(), mem_read, &d, g_scratch};
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  SaveSummary s;
  TEST_ASSERT_TRUE(h.format->summarize(h, s));
  TEST_ASSERT_EQUAL_UINT8(0, s.hof_count);
  TEST_ASSERT_EQUAL_UINT32(0, s.first_hof);
}

void test_summary_roundtrip() {
  std::vector<uint8_t> d = emerald_save();
  SaveReader r{d.size(), mem_read, &d, g_scratch};
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  SaveSummary s, back;
  TEST_ASSERT_TRUE(h.format->summarize(h, s));
  uint8_t buf[SAVE_SUMMARY_SIZE];
  save_summary_encode(s, buf);
  TEST_ASSERT_TRUE(save_summary_decode(buf, &back));
  TEST_ASSERT_EQUAL_STRING(s.name, back.name);
  TEST_ASSERT_EQUAL_UINT32(s.money, back.money);
  TEST_ASSERT_EQUAL_UINT32(s.first_hof, back.first_hof);
  TEST_ASSERT_EQUAL_UINT8(s.hof_count, back.hof_count);
  TEST_ASSERT_EQUAL_UINT32(s.hof_hash[33], back.hof_hash[33]);
  TEST_ASSERT_EQUAL_STRING(s.party[0].nick, back.party[0].nick);
  TEST_ASSERT_EQUAL_MEMORY(s.where, back.where, SAVE_WHERE_SPECIES);
  SaveHead head;
  TEST_ASSERT_TRUE(save_head_decode(buf, &head));
  TEST_ASSERT_EQUAL_STRING("Ana", head.name);
  TEST_ASSERT_EQUAL_UINT8(34, head.hof_count);
  TEST_ASSERT_EQUAL_UINT32(s.first_hof, head.first_hof);
  TEST_ASSERT_EQUAL_UINT16(3, head.box_count);
  TEST_ASSERT_EQUAL_UINT8(s.badges, head.badges);
  buf[0] = 9;  // jogo inválido
  TEST_ASSERT_FALSE(save_summary_decode(buf, &back));
  TEST_ASSERT_FALSE(save_head_decode(buf, &head));
}

// Campos que o Gen4 trouxe: 16 insígnias, "onde está" até 493, HGSS.
void test_summary_gen4_fields() {
  SaveSummary s, back;
  memset(&s, 0, sizeof(s));
  s.game = SaveGame::HeartGoldSoulSilver;
  s.badges = 0x81FF;  // Johto inteira + 2 de Kanto
  s.badge_total = 16;
  s.where[492] = 18;  // Arceus na caixa 18
  uint8_t buf[SAVE_SUMMARY_SIZE];
  save_summary_encode(s, buf);
  TEST_ASSERT_TRUE(save_summary_decode(buf, &back));
  TEST_ASSERT_EQUAL((int)SaveGame::HeartGoldSoulSilver, (int)back.game);
  TEST_ASSERT_EQUAL_HEX16(0x81FF, back.badges);
  TEST_ASSERT_EQUAL_UINT8(16, back.badge_total);
  TEST_ASSERT_EQUAL_UINT8(10, back.badge_count());
  TEST_ASSERT_EQUAL_UINT8(18, back.where_of(493));
  SaveHead head;
  TEST_ASSERT_TRUE(save_head_decode(buf, &head));
  TEST_ASSERT_EQUAL_HEX16(0x81FF, head.badges);
  TEST_ASSERT_EQUAL_UINT8(16, head.badge_total);
  buf[57] = 0;  // resumo sem total = 8 (Gen3)
  TEST_ASSERT_TRUE(save_head_decode(buf, &head));
  TEST_ASSERT_EQUAL_UINT8(8, head.badge_total);
}

// ---------- tabelas do SD ----------

void test_name_table_and_moves() {
  uint8_t h[NAME_TABLE_HEADER_SIZE];
  name_table_header_encode(354, MOVE_RECORD_SIZE, h);
  NameTableHeader hd;
  TEST_ASSERT_TRUE(name_table_header_decode(h, &hd));
  TEST_ASSERT_EQUAL_UINT16(354, hd.count);
  TEST_ASSERT_EQUAL_UINT32(NAME_TABLE_HEADER_SIZE + 56 * MOVE_RECORD_SIZE, name_table_offset(hd, 57));
  h[0] = 'X';
  TEST_ASSERT_FALSE(name_table_header_decode(h, &hd));

  uint8_t rec[MOVE_RECORD_SIZE] = {0};
  memcpy(rec, "Surf", 4);
  rec[24] = 2;
  rec[25] = 15;
  MoveInfo mi;
  TEST_ASSERT_TRUE(move_record_decode(rec, &mi));
  TEST_ASSERT_EQUAL_STRING("Surf", mi.name);
  TEST_ASSERT_EQUAL_UINT8(15, mi.pp);
  memset(rec, 'a', NAME_TABLE_NAME_SIZE);  // sem NUL
  TEST_ASSERT_FALSE(move_record_decode(rec, &mi));

  TEST_ASSERT_EQUAL_UINT8(15, move_pp_max(15, 0));
  TEST_ASSERT_EQUAL_UINT8(24, move_pp_max(15, 3));
  TEST_ASSERT_EQUAL_UINT8(8, move_pp_max(5, 3));
  TEST_ASSERT_EQUAL_UINT8(1, move_pp_max(1, 3));
}

void test_species_growth_byte() {
  SpeciesRecord r{};
  r.number = 1;
  strcpy(r.name, "Bulbasaur");
  r.type2 = SpeciesType::None;
  r.growth = (uint8_t)GrowthRate::MediumSlow;
  uint8_t buf[SPECIES_RECORD_SIZE];
  species_record_encode(r, buf);
  SpeciesRecord back;
  TEST_ASSERT_TRUE(species_record_decode(buf, 1, &back));
  TEST_ASSERT_EQUAL_UINT8(4, back.growth);
  buf[39] = 0;  // SD antigo
  TEST_ASSERT_TRUE(species_record_decode(buf, 1, &back));
  TEST_ASSERT_EQUAL_UINT8(0, back.growth);
}

void test_saves_cache_roundtrip() {
  SaveSignature sig;
  sig.add("emerald.sav", 131072, 1234);
  uint8_t h[SAVES_CACHE_HEADER_SIZE];
  saves_cache_header_encode(sig, h);
  uint16_t count = 0;
  TEST_ASSERT_TRUE(saves_cache_header_decode(h, sig, &count));
  TEST_ASSERT_EQUAL_UINT16(1, count);
  SaveSignature other;
  other.add("emerald.sav", 131072, 1235);
  TEST_ASSERT_FALSE(saves_cache_header_decode(h, other, &count));

  static SaveEntry e, back;
  memset(&e, 0, sizeof(e));
  strcpy(e.file, "emerald.sav");
  e.ok = true;
  strcpy(e.summary.name, "Ana");
  e.summary.game = SaveGame::Emerald;
  e.summary.where[259] = 3;
  static uint8_t rec[SAVES_CACHE_RECORD_SIZE];
  saves_cache_record_encode(e, rec);
  TEST_ASSERT_TRUE(saves_cache_record_decode(rec, &back));
  TEST_ASSERT_TRUE(back.ok);
  TEST_ASSERT_EQUAL_STRING("emerald.sav", back.file);
  TEST_ASSERT_EQUAL_STRING("Ana", back.summary.name);
  TEST_ASSERT_EQUAL_UINT8(3, back.summary.where_of(260));
  TEST_ASSERT_EQUAL_UINT8(3, rec[SAVES_CACHE_SUMMARY_AT + 432 + 259]);  // offset do "onde está"

  e.ok = false;
  saves_cache_record_encode(e, rec);
  TEST_ASSERT_TRUE(saves_cache_record_decode(rec, &back));
  TEST_ASSERT_FALSE(back.ok);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_species_conversion);
  RUN_TEST(test_species_conversion_is_a_bijection_on_hoenn);
  RUN_TEST(test_pk3_roundtrip_every_block_order);
  RUN_TEST(test_pk3_party_fields);
  RUN_TEST(test_pk3_rejects_empty_and_corrupt);
  RUN_TEST(test_pk3_egg_flag);
  RUN_TEST(test_shiny_formula);
  RUN_TEST(test_exp_curves_at_100);
  RUN_TEST(test_level_from_exp);
  RUN_TEST(test_stat_formula_bulbapedia_example);
  RUN_TEST(test_fill_stats_for_box_mon);
  RUN_TEST(test_save_summary_from_emerald);
  RUN_TEST(test_save_box_and_hof_reads);
  RUN_TEST(test_hof_with_bad_checksum_is_ignored);
  RUN_TEST(test_summary_roundtrip);
  RUN_TEST(test_summary_gen4_fields);
  RUN_TEST(test_name_table_and_moves);
  RUN_TEST(test_species_growth_byte);
  RUN_TEST(test_saves_cache_roundtrip);
  return UNITY_END();
}
