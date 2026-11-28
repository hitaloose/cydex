#include <unity.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "dex/DexStatus.h"
#include "saves/SaveFormat.h"
#include "saves/gen4/Gen4Layout.h"
#include "saves/gen4/Gen4Pk4.h"
#include "saves/gen4/Gen4Save.h"
#include "saves/gen4/Gen4Text.h"
#include "utils/ByteUtils.h"

void setUp() {}
void tearDown() {}

static uint8_t g_scratch[SAVE_SCRATCH_SIZE];
static const Gen4Layout &L = GEN4_LAYOUT_HGSS;

static bool mem_read(size_t off, uint8_t *out, size_t len, void *user) {
  auto *d = (std::vector<uint8_t> *)user;
  if (off + len > d->size()) return false;
  memcpy(out, d->data() + off, len);
  return true;
}

static void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

// PK4 em claro (blocos na ordem A, B, C, D) e cifrado.
static void make_pk4(uint8_t *raw, bool party, uint32_t pid, uint16_t species, const char *nick, uint8_t level) {
  memset(raw, 0, party ? GEN4_PK4_PARTY_SIZE : GEN4_PK4_STORED_SIZE);
  put32(raw, pid);
  uint8_t *a = raw + 0x08, *b = raw + 0x28, *c = raw + 0x48, *d = raw + 0x68;
  put16(a, species);
  put16(a + 0x02, 231);    // item
  put16(a + 0x04, 51139);  // TID
  put16(a + 0x06, 8593);   // SID
  put32(a + 0x08, 1000);
  a[0x10] = 252;  // EV de PS
  put16(b, 94);   // 1º golpe
  b[0x08] = 10;
  put32(b + 0x10, 31u | (31u << 15));  // IV PS = 31, IV Vel = 31
  gen4_text_encode(nick, c, 11);
  gen4_text_encode("Hitalo", d, 8);
  if (party) {
    raw[0x8C] = level;
    put16(raw + 0x8E, 100);
    for (int i = 0; i < 6; i++) put16(raw + 0x90 + 2 * i, (uint16_t)(200 + i));
  }
  gen4_pk4_encrypt(raw, party);
}

static void write_footer(std::vector<uint8_t> &d, size_t block, uint32_t size, uint32_t counter) {
  uint8_t *f = d.data() + block + size - 16;
  put32(f, counter);
  put32(f + 4, size);
  put32(f + 8, GEN4_FOOTER_MAGIC);
  put16(f + 12, 0);
  put16(f + 14, gen4_crc16(d.data() + block, size - 16));
}

// Save HGSS sintético: cópia 0 mais nova (contador 10), cópia 1 mais velha
// (contador 9) com outro nome, pra testar a escolha.
static std::vector<uint8_t> hgss_save() {
  std::vector<uint8_t> d(GEN4_SAVE_SIZE, 0);
  for (int copy = 0; copy < 2; copy++) {
    uint8_t *g = d.data() + copy * GEN4_COPY_OFFSET;
    gen4_text_encode(copy == 0 ? "Hitalo" : "Velho", g + L.trainer_name, 8);
    put16(g + L.tid, 51139);
    put16(g + L.sid, 8593);
    put32(g + L.money, 62040);
    g[L.gender] = 1;
    g[L.badges] = 0xFF;
    g[L.badges2] = 0x03;
    put16(g + L.played_hours, 41);
    g[L.played_minutes] = 21;
    g[L.played_seconds] = 27;
    g[L.party_count] = 2;
    make_pk4(g + L.party, true, 0x12345678, 196, "ESPEON", 77);
    make_pk4(g + L.party + GEN4_PK4_PARTY_SIZE, true, 0x0BADF00D, 493, "ARCEUS", 80);
    put32(g + L.dex, 0xBEEFCAFE);
    uint8_t *caught = g + L.dex + GEN4_DEX_CAUGHT, *seen = g + L.dex + GEN4_DEX_SEEN;
    for (int n : {1, 196, 493}) caught[(n - 1) / 8] |= 1 << ((n - 1) % 8);
    for (int n : {1, 2, 196, 400, 493}) seen[(n - 1) / 8] |= 1 << ((n - 1) % 8);
    caught[63] = 0xFF;  // lixo acima de 493 (aparece no save real)
    seen[63] = 0xFF;

    uint8_t *s = g + L.storage_at;
    make_pk4(s + 2 * GEN4_PK4_STORED_SIZE, false, 0x00C0FFEE, 1, "BULBASAUR", 0);
    make_pk4(s + 17 * L.box_stride, false, 0x0000BEEF, 196, "EEVEE", 0);  // espécie do time
    make_pk4(s + 17 * L.box_stride + GEN4_PK4_STORED_SIZE, false, 0x00001234, 450, "HIPPO", 0);
    gen4_text_encode("BOX 1", s + L.box_names, 20);
    gen4_text_encode("FIM", s + L.box_names + 17 * L.box_name_stride, 20);

    write_footer(d, copy * GEN4_COPY_OFFSET, L.general_size, copy == 0 ? 10 : 9);
    write_footer(d, copy * GEN4_COPY_OFFSET + L.storage_at, L.storage_size, copy == 0 ? 10 : 9);

    // Hall da Fama: 2 times.
    uint8_t *h = g + L.hof_at;
    for (int t = 0; t < 2; t++) {
      for (int i = 0; i < 6; i++) {
        uint8_t *m = h + t * GEN4_HOF_TEAM_SIZE + i * GEN4_HOF_MON_SIZE;
        put16(m, (uint16_t)(152 + i + t));
        m[2] = (uint8_t)(40 + t * 10 + i);
        put32(m + 4, 0x1000u * (t + 1) + i);
        put16(m + 8, 51139);
        put16(m + 10, 8593);
        gen4_text_encode("HOF", m + 0x0C, 11);
        gen4_text_encode("Hitalo", m + 0x22, 8);
        put16(m + 0x32, 33);
      }
    }
    put32(h + GEN4_HOF_TOTAL_AT, 2);
    put32(h + L.hof_data, GEN4_FOOTER_MAGIC);
    put32(h + L.hof_data + 4, copy == 0 ? 2 : 1);
    put32(h + L.hof_data + 8, L.hof_data);
  }
  return d;
}

static SaveReader reader(std::vector<uint8_t> &d) { return SaveReader{d.size(), mem_read, &d, g_scratch}; }

// ---------- texto e PK4 ----------

void test_text_roundtrip() {
  uint8_t buf[22];
  gen4_text_encode("Hitalo 2!", buf, 11);
  TEST_ASSERT_EQUAL_HEX16(0x132, read_u16_le(buf));  // H
  char out[24];
  gen4_text_decode(buf, 11, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Hitalo 2!", out);
}

void test_text_latin1_and_unknown() {
  uint8_t buf[8];
  put16(buf, 0x168);     // É
  put16(buf + 2, 0x0EE); // ♂
  put16(buf + 4, 0x050); // kana: sem glifo
  put16(buf + 6, 0xFFFF);
  char out[16];
  gen4_text_decode(buf, 4, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("\xC3\x89\xE2\x99\x82?", out);
}

void test_pk4_roundtrip_party() {
  uint8_t raw[GEN4_PK4_PARTY_SIZE];
  make_pk4(raw, true, 0xCB6C5D3E, 196, "ESPEON", 77);
  SaveMon m;
  TEST_ASSERT_TRUE(gen4_pk4_decode(raw, true, &m));
  TEST_ASSERT_EQUAL_UINT16(196, m.species);
  TEST_ASSERT_EQUAL_STRING("ESPEON", m.nick);
  TEST_ASSERT_EQUAL_STRING("Hitalo", m.ot);
  TEST_ASSERT_EQUAL_UINT16(231, m.item);
  TEST_ASSERT_EQUAL_UINT16(94, m.moves[0]);
  TEST_ASSERT_EQUAL_UINT8(10, m.pp[0]);
  TEST_ASSERT_EQUAL_UINT8(31, m.ivs[0]);
  TEST_ASSERT_EQUAL_UINT8(31, m.ivs[5]);  // Vel (5º no jogo) vai pro fim
  TEST_ASSERT_EQUAL_UINT8(252, m.evs[0]);
  TEST_ASSERT_EQUAL_UINT8(77, m.level);
  TEST_ASSERT_EQUAL_UINT16(100, m.hp);
  // Jogo: PS máx, Atq, Def, Vel, AtE, DfE -> PS, Atq, Def, AtE, DfE, Vel.
  TEST_ASSERT_EQUAL_UINT16(200, m.stats[0]);
  TEST_ASSERT_EQUAL_UINT16(204, m.stats[3]);
  TEST_ASSERT_EQUAL_UINT16(205, m.stats[4]);
  TEST_ASSERT_EQUAL_UINT16(203, m.stats[5]);
  TEST_ASSERT_EQUAL_UINT8(0xCB6C5D3E % 25, m.nature);
  TEST_ASSERT_FALSE(m.egg);
}

void test_pk4_rejects_bad_checksum_and_empty() {
  uint8_t raw[GEN4_PK4_STORED_SIZE];
  make_pk4(raw, false, 0x1234, 25, "PIKA", 0);
  raw[0x20] ^= 0x55;
  SaveMon m;
  TEST_ASSERT_FALSE(gen4_pk4_decode(raw, false, &m));
  memset(raw, 0, sizeof(raw));
  TEST_ASSERT_FALSE(gen4_pk4_decode(raw, false, &m));
}

void test_crc16_known_value() {
  // CRC-16/CCITT-FALSE de "123456789" = 0x29B1.
  TEST_ASSERT_EQUAL_HEX16(0x29B1, gen4_crc16((const uint8_t *)"123456789", 9));
  // Incremental = de uma vez.
  uint16_t c = gen4_crc16((const uint8_t *)"1234", 4);
  TEST_ASSERT_EQUAL_HEX16(0x29B1, gen4_crc16((const uint8_t *)"56789", 5, c));
}

// ---------- save ----------

void test_detect_and_open() {
  auto d = hgss_save();
  SaveReader r = reader(d);
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  TEST_ASSERT_EQUAL_STRING("gen4", h.format->name);
  std::vector<uint8_t> small(128 * 1024, 0);
  SaveReader r3 = reader(small);
  TEST_ASSERT_FALSE(GEN4_SAVE_FORMAT.detect(r3));
  std::vector<uint8_t> blank(GEN4_SAVE_SIZE, 0);  // 512 KB sem rodapé (ex.: Gen5)
  SaveReader r5 = reader(blank);
  TEST_ASSERT_FALSE(GEN4_SAVE_FORMAT.detect(r5));
}

void test_summary() {
  auto d = hgss_save();
  SaveReader r = reader(d);
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  SaveSummary s;
  TEST_ASSERT_TRUE(h.format->summarize(h, s));
  TEST_ASSERT_EQUAL((int)SaveGame::HeartGoldSoulSilver, (int)s.game);
  TEST_ASSERT_EQUAL_STRING("Hitalo", s.name);  // a cópia mais nova
  TEST_ASSERT_EQUAL_UINT16(51139, s.tid);
  TEST_ASSERT_EQUAL_UINT32(62040, s.money);
  TEST_ASSERT_EQUAL_UINT8(1, s.gender);
  TEST_ASSERT_EQUAL_HEX16(0x03FF, s.badges);
  TEST_ASSERT_EQUAL_UINT8(16, s.badge_total);
  TEST_ASSERT_EQUAL_UINT8(10, s.badge_count());
  TEST_ASSERT_EQUAL_UINT16(41, s.hours);
  TEST_ASSERT_EQUAL_UINT8(21, s.minutes);
  TEST_ASSERT_EQUAL_UINT16(493, s.dex_total);
  TEST_ASSERT_EQUAL_UINT16(3, s.caught);  // sem o lixo acima de 493
  TEST_ASSERT_EQUAL_UINT16(5, s.seen);
  TEST_ASSERT_EQUAL_UINT8(2, s.party_count);
  TEST_ASSERT_EQUAL_UINT16(493, s.party[1].species);
  TEST_ASSERT_EQUAL_UINT8(80, s.party[1].level);
  TEST_ASSERT_EQUAL_UINT8(18, s.box_total);
  TEST_ASSERT_EQUAL_UINT16(3, s.box_count);
  TEST_ASSERT_EQUAL_UINT8(SAVE_WHERE_PARTY, s.where_of(196));  // o time vence a caixa
  TEST_ASSERT_EQUAL_UINT8(1, s.where_of(1));
  TEST_ASSERT_EQUAL_UINT8(18, s.where_of(450));
  TEST_ASSERT_EQUAL_UINT8(2, s.hof_count);
  TEST_ASSERT_NOT_EQUAL(s.hof_hash[0], s.hof_hash[1]);
  TEST_ASSERT_EQUAL_UINT32(0, s.first_hof);
}

void test_newer_copy_with_bad_crc_falls_back() {
  auto d = hgss_save();
  d[L.trainer_name + 20] ^= 0xFF;  // estraga a cópia 0 (contador 10) sem refazer o CRC
  SaveReader r = reader(d);
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  SaveSummary s;
  TEST_ASSERT_TRUE(h.format->summarize(h, s));
  TEST_ASSERT_EQUAL_STRING("Velho", s.name);
}

void test_merge_dex() {
  auto d = hgss_save();
  SaveReader r = reader(d);
  DexStatus dex{};
  TEST_ASSERT_NOT_NULL(save_merge_dex(r, dex));
  TEST_ASSERT_TRUE(dex.is_caught(493));
  TEST_ASSERT_TRUE(dex.is_seen(400));
  TEST_ASSERT_FALSE(dex.is_caught(400));
  TEST_ASSERT_FALSE(dex.is_seen(500));
}

void test_box_and_hof() {
  auto d = hgss_save();
  SaveReader r = reader(d);
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  SaveMon box[SAVE_BOX_SLOTS];
  char name[24];
  TEST_ASSERT_TRUE(h.format->read_box(h, 0, box, name, sizeof(name)));
  TEST_ASSERT_EQUAL_STRING("BOX 1", name);
  TEST_ASSERT_EQUAL_UINT16(0, box[0].species);
  TEST_ASSERT_EQUAL_UINT16(1, box[2].species);
  TEST_ASSERT_EQUAL_UINT8(0, box[2].level);  // caixa: nível vem do MonStats
  TEST_ASSERT_TRUE(h.format->read_box(h, 17, box, name, sizeof(name)));
  TEST_ASSERT_EQUAL_STRING("FIM", name);
  TEST_ASSERT_FALSE(h.format->read_box(h, 18, box, name, sizeof(name)));

  SaveMon team[6];
  TEST_ASSERT_EQUAL_UINT8(6, h.format->read_hof_team(h, 1, team));
  TEST_ASSERT_EQUAL_UINT16(153, team[0].species);
  TEST_ASSERT_EQUAL_UINT8(50, team[0].level);
  TEST_ASSERT_EQUAL_STRING("HOF", team[0].nick);
  TEST_ASSERT_EQUAL_STRING("Hitalo", team[0].ot);
  TEST_ASSERT_EQUAL_UINT8(0, h.format->read_hof_team(h, 2, team));

  SaveMon party[6];
  TEST_ASSERT_EQUAL_UINT8(2, h.format->read_party(h, party));
  TEST_ASSERT_EQUAL_STRING("ARCEUS", party[1].nick);
}

// ---------- save real (opcional) ----------
// CYDEX_TEST_SAVE_HGSS=<caminho do .sav> imprime o resumo pra conferir à mão.

void test_real_save() {
  const char *path = getenv("CYDEX_TEST_SAVE_HGSS");
  if (!path || !*path) TEST_IGNORE_MESSAGE("CYDEX_TEST_SAVE_HGSS não definido");
  FILE *f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, "não abriu o save");
  std::vector<uint8_t> d;
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.insert(d.end(), buf, buf + n);
  fclose(f);

  SaveReader r = reader(d);
  SaveHandle h;
  TEST_ASSERT_TRUE(save_open(r, &h));
  TEST_ASSERT_EQUAL_STRING("gen4", h.format->name);
  SaveSummary s;
  TEST_ASSERT_TRUE(h.format->summarize(h, s));
  printf("\n%s TID %u · %uh%02u · $%u · insígnias %u/%u (0x%04X) · dex %u/%u vistos %u\n", s.name, s.tid,
         s.hours, s.minutes, (unsigned)s.money, s.badge_count(), s.badge_total, s.badges, s.caught, s.dex_total,
         s.seen);
  printf("caixas %u (%u criaturas) · brilhantes %u · Hall %u\n", s.box_total, s.box_count, s.shiny_count,
         s.hof_count);
  SaveMon party[6];
  uint8_t pc = h.format->read_party(h, party);
  for (uint8_t i = 0; i < pc; i++)
    printf("  time %u: #%u %s Nv %u PS %u/%u item %u golpes %u %u %u %u\n", i, party[i].species, party[i].nick,
           party[i].level, party[i].hp, party[i].stats[0], party[i].item, party[i].moves[0], party[i].moves[1],
           party[i].moves[2], party[i].moves[3]);
  SaveMon box[SAVE_BOX_SLOTS];
  char name[24];
  TEST_ASSERT_TRUE(h.format->read_box(h, 0, box, name, sizeof(name)));
  printf("  %s:", name);
  for (auto &m : box)
    if (m.species) printf(" #%u %s", m.species, m.nick);
  printf("\n");
  for (uint8_t t = 0; t < s.hof_count; t++) {
    SaveMon team[6];
    uint8_t k = h.format->read_hof_team(h, t, team);
    printf("  Hall #%u:", t + 1);
    for (uint8_t i = 0; i < k; i++) printf(" #%u Nv %u", team[i].species, team[i].level);
    printf("\n");
  }
  TEST_ASSERT_EQUAL_UINT8(pc, s.party_count);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_text_roundtrip);
  RUN_TEST(test_text_latin1_and_unknown);
  RUN_TEST(test_pk4_roundtrip_party);
  RUN_TEST(test_pk4_rejects_bad_checksum_and_empty);
  RUN_TEST(test_crc16_known_value);
  RUN_TEST(test_detect_and_open);
  RUN_TEST(test_summary);
  RUN_TEST(test_newer_copy_with_bad_crc_falls_back);
  RUN_TEST(test_merge_dex);
  RUN_TEST(test_box_and_hof);
  RUN_TEST(test_real_save);
  return UNITY_END();
}
