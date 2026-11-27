#include <unity.h>

#include <cstring>
#include <vector>

#include "dex/Dex.h"
#include "dex/DexStatus.h"
#include "dex/SpeciesRecord.h"
#include "dex/SpriteFile.h"
#include "dex/StatusCache.h"
#include "logic/Catalog.h"
#include "logic/Format.h"
#include "logic/NumberInput.h"
#include "logic/Pager.h"
#include "logic/RowPages.h"
#include "logic/Search.h"
#include "logic/SettingsPatch.h"
#include "logic/Showcase.h"
#include "logic/Sun.h"
#include "logic/WakeLease.h"
#include "saves/SaveFormat.h"
#include "saves/SaveName.h"
#include "saves/gen3/Gen3Sections.h"
#include "utils/ByteUtils.h"

static uint8_t g_scratch[SAVE_SCRATCH_SIZE];

void setUp() {}
void tearDown() {}

// ---------- Dex / DexStatus ----------

void test_generation_boundaries() {
  TEST_ASSERT_EQUAL(0, dex_generation_of(0));
  TEST_ASSERT_EQUAL(1, dex_generation_of(1));
  TEST_ASSERT_EQUAL(1, dex_generation_of(151));
  TEST_ASSERT_EQUAL(2, dex_generation_of(152));
  TEST_ASSERT_EQUAL(3, dex_generation_of(386));
  TEST_ASSERT_EQUAL(4, dex_generation_of(387));
  TEST_ASSERT_EQUAL(8, dex_generation_of(905));
  TEST_ASSERT_EQUAL(9, dex_generation_of(906));
  TEST_ASSERT_EQUAL(9, dex_generation_of(1025));
  TEST_ASSERT_EQUAL(0, dex_generation_of(1026));
}

void test_status_caught_implies_seen() {
  DexStatus s;
  s.clear();
  s.mark_caught(25);
  s.mark_seen(1025);
  s.mark_caught(0);     // ignorado
  s.mark_caught(1026);  // ignorado
  TEST_ASSERT_TRUE(s.is_seen(25));
  TEST_ASSERT_TRUE(s.is_caught(25));
  TEST_ASSERT_TRUE(s.state(25) == SpeciesState::Caught);
  TEST_ASSERT_TRUE(s.state(1025) == SpeciesState::Seen);
  TEST_ASSERT_TRUE(s.state(1) == SpeciesState::Unknown);
  TEST_ASSERT_EQUAL(2, s.count_seen());
  TEST_ASSERT_EQUAL(1, s.count_caught());
  TEST_ASSERT_EQUAL(1, s.count_caught_range(1, 151));
  TEST_ASSERT_EQUAL(0, s.count_caught_range(152, 251));
}

void test_status_merge() {
  DexStatus a, b;
  a.clear();
  b.clear();
  a.mark_caught(1);
  b.mark_caught(4);
  b.mark_seen(7);
  a.merge(b);
  TEST_ASSERT_TRUE(a.is_caught(1));
  TEST_ASSERT_TRUE(a.is_caught(4));
  TEST_ASSERT_TRUE(a.is_seen(7));
  TEST_ASSERT_FALSE(a.is_caught(7));
}

// ---------- SpeciesRecord ----------

void test_species_record_roundtrip() {
  SpeciesRecord r{};
  strcpy(r.name, "Flabébé");
  r.type1 = SpeciesType::Fairy;
  r.type2 = SpeciesType::None;
  r.height_dm = 1;
  r.weight_hg = 1;
  r.generation = 6;
  uint8_t stats[6] = {44, 38, 39, 61, 79, 42};
  memcpy(r.stats, stats, 6);
  r.evo_count = 3;
  r.evo[0] = 669;
  r.evo[1] = 670;
  r.evo[2] = 671;
  r.evo_level[0] = 19;
  r.evo_level[1] = 0;
  r.flags = SPECIES_FLAG_HAS_SPRITE;

  uint8_t buf[SPECIES_RECORD_SIZE];
  species_record_encode(r, buf);
  SpeciesRecord d;
  TEST_ASSERT_TRUE(species_record_decode(buf, 669, &d));
  TEST_ASSERT_EQUAL_STRING("Flabébé", d.name);
  TEST_ASSERT_EQUAL(669, d.number);
  TEST_ASSERT_TRUE(d.type1 == SpeciesType::Fairy);
  TEST_ASSERT_TRUE(d.type2 == SpeciesType::None);
  TEST_ASSERT_EQUAL(303, d.stat_total());
  TEST_ASSERT_EQUAL(671, d.evo[2]);
  TEST_ASSERT_EQUAL(19, d.evo_level[0]);
  TEST_ASSERT_EQUAL(SPECIES_FLAG_HAS_SPRITE, d.flags);
}

void test_species_record_rejects_bad_type_and_unterminated_name() {
  uint8_t buf[SPECIES_RECORD_SIZE] = {0};
  strcpy((char *)buf, "Bulbasaur");
  buf[16] = 0xFF;  // tipo 1 não pode ser "nenhum"
  SpeciesRecord d;
  TEST_ASSERT_FALSE(species_record_decode(buf, 1, &d));
  buf[16] = 3;
  buf[17] = 7;
  TEST_ASSERT_TRUE(species_record_decode(buf, 1, &d));
  memset(buf, 'A', SPECIES_NAME_SIZE);  // sem NUL
  TEST_ASSERT_FALSE(species_record_decode(buf, 1, &d));
}

void test_species_header() {
  uint8_t h[SPECIES_HEADER_SIZE];
  species_header_encode(1025, h);
  SpeciesFileHeader out;
  TEST_ASSERT_TRUE(species_header_decode(h, &out));
  TEST_ASSERT_EQUAL(1025, out.count);
  TEST_ASSERT_EQUAL(16 + 64 * 24, species_record_offset(25));
  h[0] = 'X';
  TEST_ASSERT_FALSE(species_header_decode(h, &out));
}

void test_sprite_file_header_and_offsets() {
  uint8_t h[SPRITE_HEADER_SIZE] = {'C', 'Y', 'S', 'P', 1, 0, 0x01, 0x04, 48, 0};
  uint32_t rec = 4 + 48 * 48 * 2;  // 4612
  h[10] = rec & 0xFF;
  h[11] = (rec >> 8) & 0xFF;
  SpriteFileHeader out;
  TEST_ASSERT_TRUE(sprite_header_decode(h, &out));
  TEST_ASSERT_EQUAL(1025, out.count);
  TEST_ASSERT_EQUAL(48, out.px);
  TEST_ASSERT_EQUAL(16, sprite_record_offset(48, 1));
  TEST_ASSERT_EQUAL(16 + 4612 * 24, sprite_record_offset(48, 25));
  h[10] ^= 1;  // tamanho de registro incoerente
  TEST_ASSERT_FALSE(sprite_header_decode(h, &out));
}

// ---------- StatusCache ----------

void test_signature_is_order_independent() {
  SaveSignature a, b;
  a.add("emerald.sav", 131072, 100);
  a.add("fr.srm", 131072, 200);
  b.add("fr.srm", 131072, 200);
  b.add("emerald.sav", 131072, 100);
  TEST_ASSERT_EQUAL_UINT32(a.value, b.value);
  SaveSignature c;
  c.add("emerald.sav", 131072, 101);  // data mudou
  c.add("fr.srm", 131072, 200);
  TEST_ASSERT_NOT_EQUAL(a.value, c.value);
}

void test_cache_roundtrip_and_invalidation() {
  DexStatus st;
  st.clear();
  st.mark_caught(150);
  st.mark_seen(151);
  SaveSignature sig;
  sig.add("a.sav", 131072, 1);

  uint8_t buf[STATUS_CACHE_SIZE];
  status_cache_encode(sig, st, buf);

  DexStatus out;
  out.clear();
  TEST_ASSERT_TRUE(status_cache_decode(buf, sizeof(buf), sig, &out));
  TEST_ASSERT_TRUE(out.is_caught(150));
  TEST_ASSERT_TRUE(out.is_seen(151));

  SaveSignature other;
  other.add("a.sav", 131072, 2);
  TEST_ASSERT_FALSE(status_cache_decode(buf, sizeof(buf), other, &out));

  SaveSignature more = sig;
  more.add("b.sav", 131072, 1);
  TEST_ASSERT_FALSE(status_cache_decode(buf, sizeof(buf), more, &out));
  TEST_ASSERT_FALSE(status_cache_decode(buf, sizeof(buf) - 1, sig, &out));
}

// ---------- Pager / HoldRepeat ----------

void test_pager_basics() {
  Pager p;
  p.reset(1025, 4);
  TEST_ASSERT_EQUAL(257, p.page_count());
  TEST_ASSERT_FALSE(p.has_prev());
  TEST_ASSERT_FALSE(p.prev());
  TEST_ASSERT_TRUE(p.next());
  TEST_ASSERT_EQUAL(4, p.first_index());
  TEST_ASSERT_TRUE(p.go_to_page(9999));  // clampa na última
  TEST_ASSERT_EQUAL(256, p.page);
  TEST_ASSERT_EQUAL(1, p.items_on_page());
  TEST_ASSERT_FALSE(p.next());
  TEST_ASSERT_TRUE(p.go_to_index(24));  // #25 -> página 6
  TEST_ASSERT_EQUAL(6, p.page);

  p.reset(0, 10);
  TEST_ASSERT_EQUAL(1, p.page_count());
  TEST_ASSERT_EQUAL(0, p.items_on_page());
}

void test_hold_repeat_accelerates() {
  HoldRepeat h;
  h.press(0);  // passo 1 já no toque
  TEST_ASSERT_FALSE(h.poll(449));
  TEST_ASSERT_TRUE(h.poll(450));  // passo 2: espera inicial de 450 ms
  uint32_t t = 450;
  for (int step = 3; step <= 8; step++) {
    t += 250;
    TEST_ASSERT_FALSE(h.poll(t - 1));
    TEST_ASSERT_TRUE(h.poll(t));
  }
  // a partir do 8o passo, 90 ms
  TEST_ASSERT_FALSE(h.poll(t + 89));
  TEST_ASSERT_TRUE(h.poll(t + 90));
  h.release();
  TEST_ASSERT_FALSE(h.poll(t + 10000));
}

// ---------- Catalog ----------

// Tipos falsos: pares = Fogo, múltiplos de 3 também Voador; resto Água.
static void fake_types(uint16_t n, SpeciesType *t1, SpeciesType *t2) {
  *t1 = (n % 2 == 0) ? SpeciesType::Fire : SpeciesType::Water;
  *t2 = (n % 3 == 0) ? SpeciesType::Flying : SpeciesType::None;
}

void test_catalog_status_filters() {
  DexStatus st;
  st.clear();
  st.mark_caught(2);
  st.mark_caught(5);
  st.mark_seen(3);
  uint16_t out[10];
  CatalogQuery q;
  TEST_ASSERT_EQUAL(10, catalog_build(q, 10, st, fake_types, out));
  q.status = StatusFilter::Caught;
  TEST_ASSERT_EQUAL(2, catalog_build(q, 10, st, fake_types, out));
  TEST_ASSERT_EQUAL(2, out[0]);
  TEST_ASSERT_EQUAL(5, out[1]);
  q.status = StatusFilter::Missing;  // tudo que não está capturado (visto entra)
  TEST_ASSERT_EQUAL(8, catalog_build(q, 10, st, fake_types, out));
  TEST_ASSERT_EQUAL(1, out[0]);
  TEST_ASSERT_EQUAL(3, out[1]);
}

void test_catalog_type_filters() {
  DexStatus st;
  st.clear();
  uint16_t out[12];
  CatalogQuery q;
  TEST_ASSERT_TRUE(q.toggle_type(SpeciesType::Fire));
  TEST_ASSERT_EQUAL(6, catalog_build(q, 12, st, fake_types, out));  // 2,4,...,12
  TEST_ASSERT_TRUE(q.toggle_type(SpeciesType::Flying));
  TEST_ASSERT_EQUAL(2, catalog_build(q, 12, st, fake_types, out));  // 6, 12
  TEST_ASSERT_EQUAL(6, out[0]);
  TEST_ASSERT_FALSE(q.toggle_type(SpeciesType::Water));  // já tem 2
  TEST_ASSERT_TRUE(q.toggle_type(SpeciesType::Fire));    // tira Fogo
  TEST_ASSERT_TRUE(q.type_a == SpeciesType::Flying);
  TEST_ASSERT_EQUAL(1, q.type_count());
}

void test_catalog_index_at_or_after() {
  uint16_t list[] = {3, 7, 10, 150};
  TEST_ASSERT_EQUAL(0, catalog_index_at_or_after(list, 4, 1));
  TEST_ASSERT_EQUAL(1, catalog_index_at_or_after(list, 4, 7));
  TEST_ASSERT_EQUAL(3, catalog_index_at_or_after(list, 4, 11));
  TEST_ASSERT_EQUAL(3, catalog_index_at_or_after(list, 4, 999));  // passou: último
  TEST_ASSERT_EQUAL(0, catalog_index_at_or_after(list, 0, 5));
}

void test_catalog_neighbors() {
  uint16_t list[] = {3, 7, 10, 150};
  uint16_t p, n;
  catalog_neighbors(list, 4, 3, &p, &n);
  TEST_ASSERT_EQUAL(0, p);  // primeiro: ‹ apagado
  TEST_ASSERT_EQUAL(7, n);
  catalog_neighbors(list, 4, 10, &p, &n);
  TEST_ASSERT_EQUAL(7, p);
  TEST_ASSERT_EQUAL(150, n);
  catalog_neighbors(list, 4, 150, &p, &n);
  TEST_ASSERT_EQUAL(10, p);
  TEST_ASSERT_EQUAL(0, n);  // último: › apagado
  catalog_neighbors(list, 4, 8, &p, &n);  // fora da lista (aberto pela evolução)
  TEST_ASSERT_EQUAL(7, p);
  TEST_ASSERT_EQUAL(10, n);
  catalog_neighbors(list, 4, 1, &p, &n);
  TEST_ASSERT_EQUAL(0, p);
  TEST_ASSERT_EQUAL(3, n);
  catalog_neighbors(list, 0, 5, &p, &n);
  TEST_ASSERT_EQUAL(0, p);
  TEST_ASSERT_EQUAL(0, n);
}

void test_format_tenths() {
  char buf[12];
  format_tenths(7, ",", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("0,7", buf);
  format_tenths(69, ".", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("6.9", buf);
  format_tenths(9999, ",", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("999,9", buf);
  format_tenths(0, ",", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("0,0", buf);
}

void test_search_fold() {
  char out[32];
  search_fold("Mr. Mime", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("mrmime", out);
  search_fold("Flab\xC3\xA9" "b\xC3\xA9", out, sizeof(out));  // Flabébé
  TEST_ASSERT_EQUAL_STRING("flabebe", out);
  search_fold("Type: Null", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("typenull", out);
  search_fold("Farfetch'd", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("farfetchd", out);
  search_fold("\xC3\x87\xC3\xA3o", out, sizeof(out));  // Ção
  TEST_ASSERT_EQUAL_STRING("cao", out);
  TEST_ASSERT_EQUAL(3, search_fold("abcdef", out, 4));  // corta no tamanho
  TEST_ASSERT_EQUAL_STRING("abc", out);
}

void test_search_matches() {
  SearchKey k;
  TEST_ASSERT_FALSE(search_key_make(" .-", &k));  // só pontuação: nada a buscar
  TEST_ASSERT_TRUE(search_key_make("BUL", &k));
  TEST_ASSERT_FALSE(k.numeric);
  TEST_ASSERT_TRUE(search_matches(k, 1, "Bulbasaur", true));
  TEST_ASSERT_TRUE(search_matches(k, 209, "Snubbull", true));
  TEST_ASSERT_FALSE(search_matches(k, 1, "Bulbasaur", false));  // não visto: nome não casa
  TEST_ASSERT_TRUE(search_key_make("mr mime", &k));
  TEST_ASSERT_TRUE(search_matches(k, 122, "Mr. Mime", true));

  TEST_ASSERT_TRUE(search_key_make("25", &k));
  TEST_ASSERT_TRUE(k.numeric);
  TEST_ASSERT_TRUE(search_matches(k, 25, "Pikachu", false));  // número vale pra qualquer um
  TEST_ASSERT_TRUE(search_matches(k, 250, "Ho-Oh", true));
  TEST_ASSERT_TRUE(search_matches(k, 125, "Electabuzz", true));
  TEST_ASSERT_FALSE(search_matches(k, 52, "Meowth", true));
  TEST_ASSERT_TRUE(search_key_make("001", &k));
  TEST_ASSERT_TRUE(search_matches(k, 1, nullptr, false));
}

const char *fake_names(uint16_t n) {
  static const char *const names[] = {"", "Bulbasaur", "Ivysaur", "Venusaur", "Snubbull"};
  return n < 5 ? names[n] : "";
}

void test_catalog_text_and_pick_type() {
  DexStatus st;
  st.clear();
  st.mark_seen(1);
  st.mark_seen(2);
  st.mark_seen(4);  // 3 não visto
  CatalogQuery q;
  strcpy(q.text, "saur");
  uint16_t out[8];
  TEST_ASSERT_EQUAL(2, catalog_build(q, 4, st, fake_types, out, fake_names));  // 1, 2 (3 escondido)
  TEST_ASSERT_EQUAL(1, out[0]);
  TEST_ASSERT_EQUAL(2, out[1]);
  TEST_ASSERT_TRUE(q.refined());
  q.clear_refinements();
  TEST_ASSERT_FALSE(q.refined());

  q.pick_type(SpeciesType::Grass);
  q.pick_type(SpeciesType::Poison);
  q.pick_type(SpeciesType::Fire);  // cheio: sai Grama (o mais antigo)
  TEST_ASSERT_TRUE(q.type_a == SpeciesType::Poison);
  TEST_ASSERT_TRUE(q.type_b == SpeciesType::Fire);
  q.pick_type(SpeciesType::Poison);  // tocar de novo tira
  TEST_ASSERT_TRUE(q.type_a == SpeciesType::Fire);
  TEST_ASSERT_EQUAL(1, q.type_count());
}

void test_option_step() {
  TEST_ASSERT_EQUAL(30, option_step(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, 60, -1));
  TEST_ASSERT_EQUAL(30, option_step(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, 30, -1));  // ponta
  TEST_ASSERT_EQUAL(900, option_step(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, 900, 1));
  TEST_ASSERT_EQUAL(120, option_step(SHOWCASE_INTERVALS_S, SHOWCASE_INTERVAL_COUNT, 60, 1));
  TEST_ASSERT_EQUAL(0, option_step(SCREEN_OFF_MIN, SCREEN_OFF_COUNT, 10, 1));  // "nunca"
  TEST_ASSERT_EQUAL(0, option_index(SCREEN_OFF_MIN, SCREEN_OFF_COUNT, 7));     // inválido -> 1o
}

void test_showcase_next() {
  TEST_ASSERT_EQUAL(0, showcase_next(-1, 5, false, 0));
  TEST_ASSERT_EQUAL(3, showcase_next(2, 5, false, 0));
  TEST_ASSERT_EQUAL(0, showcase_next(4, 5, false, 0));  // volta ao começo
  for (uint32_t r = 0; r < 50; r++) {
    uint16_t n = showcase_next(2, 5, true, r * 7919);
    TEST_ASSERT_TRUE(n < 5);
    TEST_ASSERT_NOT_EQUAL(2, n);  // aleatória nunca repete
  }
  TEST_ASSERT_EQUAL(0, showcase_next(0, 1, true, 123));
  TEST_ASSERT_EQUAL(0, showcase_next(-1, 0, false, 0));
}

void test_showcase_pick() {
  // 161 espécies espalhadas: uma rodada mostra todas, cada uma uma vez.
  uint16_t list[161];
  for (uint16_t i = 0; i < 161; i++) list[i] = 1 + i * 6;
  uint8_t shown[DEX_BITS_BYTES] = {};
  uint32_t rnd = 12345;
  auto next_rnd = [&]() { rnd = rnd * 1103515245u + 12345u; return rnd >> 8; };
  uint16_t last = 0;
  for (int round = 0; round < 3; round++) {
    uint8_t seen_pos[161] = {};
    for (int k = 0; k < 161; k++) {
      uint16_t p = showcase_pick(list, 161, shown, last, next_rnd());
      TEST_ASSERT_TRUE(p < 161);
      if (k == 0) TEST_ASSERT_NOT_EQUAL(last, list[p]);  // rodada nova não repete a última
      TEST_ASSERT_EQUAL(0, seen_pos[p]);
      seen_pos[p] = 1;
      last = list[p];
    }
  }
  // Uma só: sempre ela.
  uint8_t one[DEX_BITS_BYTES] = {};
  uint16_t solo[1] = {25};
  for (int k = 0; k < 3; k++) TEST_ASSERT_EQUAL(0, showcase_pick(solo, 1, one, 25, k));
  // Lista muda no meio da rodada: a nova entra, as já mostradas continuam fora.
  uint8_t bag[DEX_BITS_BYTES] = {};
  uint16_t a[3] = {1, 2, 3};
  uint16_t first = a[showcase_pick(a, 3, bag, 0, 0)];
  uint16_t b[4] = {1, 2, 3, 4};
  uint8_t got[5] = {};
  got[first] = 1;
  for (int k = 0; k < 3; k++) {
    uint16_t n = b[showcase_pick(b, 4, bag, first, k * 7)];
    TEST_ASSERT_EQUAL(0, got[n]);
    got[n] = 1;
  }
}

void test_showcase_shift_and_cycle() {
  int8_t dx, dy;
  showcase_shift(0, &dx, &dy);
  TEST_ASSERT_EQUAL(0, dx);
  showcase_shift(5, &dx, &dy);  // 5 % 4 = 1
  TEST_ASSERT_EQUAL(2, dx);
  TEST_ASSERT_EQUAL(1, dy);
  TEST_ASSERT_EQUAL(312, showcase_cycle_minutes(312, 60));
  TEST_ASSERT_EQUAL(1, showcase_cycle_minutes(1, 30));  // arredonda pra cima
}

void test_showcase_crop() {
  constexpr uint16_t K = 0x07E0;
  static uint16_t px[96 * 96];
  for (auto &v : px) v = K;
  uint8_t x0 = 0, x1 = 0, y0 = 0, y1 = 0;
  TEST_ASSERT_FALSE(showcase_opaque_box(px, 96, K, &x0, &x1, &y0, &y1));
  px[10 * 96 + 30] = 0x1234;  // assimétrico: colunas 30 e 71 em linhas diferentes
  px[80 * 96 + 71] = 0;
  TEST_ASSERT_TRUE(showcase_opaque_box(px, 96, K, &x0, &x1, &y0, &y1));
  TEST_ASSERT_EQUAL(30, x0);
  TEST_ASSERT_EQUAL(71, x1);
  TEST_ASSERT_EQUAL(10, y0);
  TEST_ASSERT_EQUAL(80, y1);
  px[50 * 96 + 0] = 0;
  px[51 * 96 + 95] = 0;
  TEST_ASSERT_TRUE(showcase_opaque_box(px, 96, K, &x0, &x1, &y0, &y1));
  TEST_ASSERT_EQUAL(0, x0);
  TEST_ASSERT_EQUAL(95, x1);
  TEST_ASSERT_EQUAL(10, y0);
  TEST_ASSERT_EQUAL(80, y1);

  constexpr uint8_t W = SHOWCASE_CROP;
  TEST_ASSERT_EQUAL(12, showcase_crop_origin(30, 65, 96, W, true));   // pequena: centrada (48 - 36)
  TEST_ASSERT_EQUAL(0, showcase_crop_origin(0, 40, 96, W, true));     // encostada em cima: presa em 0
  TEST_ASSERT_EQUAL(24, showcase_crop_origin(70, 95, 96, W, false));  // encostada embaixo: presa em 24
  TEST_ASSERT_EQUAL(18, showcase_crop_origin(10, 89, 96, W, true));   // mais alta: fim no pé (90 - 72)
  TEST_ASSERT_EQUAL(14, showcase_crop_origin(10, 89, 96, W, false));  // mais larga: centrada
  TEST_ASSERT_EQUAL(24, showcase_crop_origin(0, 95, 96, W, true));    // 96 inteiros: pé embaixo
  TEST_ASSERT_EQUAL(0, showcase_crop_origin(0, 95, 48, 96, true));    // janela maior que o eixo
}

// ---------- NumberInput ----------

void test_number_input() {
  NumberInput in;
  char buf[8];
  in.format(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("#___", buf);
  TEST_ASSERT_FALSE(in.press_digit(0));  // zero à esquerda não entra
  TEST_ASSERT_TRUE(in.press_digit(2));
  TEST_ASSERT_TRUE(in.press_digit(5));
  in.format(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("#025", buf);
  TEST_ASSERT_TRUE(in.press_digit(0));   // 250
  TEST_ASSERT_FALSE(in.press_digit(9));  // 2509 > 1025
  TEST_ASSERT_EQUAL(250, in.value);
  in.backspace();
  in.backspace();
  TEST_ASSERT_EQUAL(2, in.value);
  in.clear();
  TEST_ASSERT_TRUE(in.press_digit(1));
  TEST_ASSERT_TRUE(in.press_digit(0));
  TEST_ASSERT_TRUE(in.press_digit(2));
  TEST_ASSERT_TRUE(in.press_digit(5));
  in.format(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("#1025", buf);
}

// ---------- Saves: registro de formatos + Gen3 ----------

static bool mem_read(size_t off, uint8_t *out, size_t len, void *user) {
  auto *v = (std::vector<uint8_t> *)user;
  if (off + len > v->size()) return false;
  memcpy(out, v->data() + off, len);
  return true;
}

// Save Gen3 sintético: slot 0 íntegro, seções em ordem física embaralhada
// (o jogo roda as seções), slot 1 vazio.
static std::vector<uint8_t> build_gen3(void (*cfg)(uint8_t *small)) {
  std::vector<uint8_t> data(GEN3_SAVE_FILE_SIZE, 0);
  for (int phys = 0; phys < GEN3_SECTIONS_PER_SLOT; phys++) {
    int id = (phys + 5) % GEN3_SECTIONS_PER_SLOT;
    uint8_t *sec = data.data() + phys * GEN3_SECTOR_SIZE;
    if (id == 0) cfg(sec);
    sec[GEN3_SECTOR_FOOTER_ID_OFFSET] = (uint8_t)id;
    uint16_t chk = gen3_checksum32(sec, GEN3_SECTOR_DATA_SIZE);
    sec[GEN3_SECTOR_FOOTER_CHECKSUM_OFFSET] = (uint8_t)chk;
    sec[GEN3_SECTOR_FOOTER_CHECKSUM_OFFSET + 1] = (uint8_t)(chk >> 8);
    sec[GEN3_SECTOR_FOOTER_COUNTER_OFFSET] = 7;
  }
  return data;
}

static void set_bit(uint8_t *small, int base, int n) { small[base + ((n - 1) >> 3)] |= 1 << ((n - 1) & 7); }

static void cfg_dex(uint8_t *small) {
  small[GEN3_SMALL_VERSION_PROBE_OFFSET] = 1;  // FRLG
  set_bit(small, GEN3_SMALL_POKEDEX_CAUGHT_OFFSET, 1);
  set_bit(small, GEN3_SMALL_POKEDEX_SEEN_OFFSET, 1);
  set_bit(small, GEN3_SMALL_POKEDEX_SEEN_OFFSET, 150);
  set_bit(small, GEN3_SMALL_POKEDEX_CAUGHT_OFFSET, 386);
}

void test_gen3_merge_dex() {
  auto data = build_gen3(cfg_dex);
  SaveReader r{data.size(), mem_read, &data, g_scratch};
  DexStatus st;
  st.clear();
  const char *fmt = save_merge_dex(r, st);
  TEST_ASSERT_NOT_NULL(fmt);
  TEST_ASSERT_EQUAL_STRING("gen3", fmt);
  TEST_ASSERT_TRUE(st.is_caught(1));
  TEST_ASSERT_TRUE(st.is_seen(150));
  TEST_ASSERT_FALSE(st.is_caught(150));
  TEST_ASSERT_TRUE(st.is_caught(386));
  TEST_ASSERT_EQUAL(3, st.count_seen());
  TEST_ASSERT_EQUAL(2, st.count_caught());
}

void test_gen3_tolerates_trailing_bytes_and_rejects_other_sizes() {
  auto data = build_gen3(cfg_dex);
  data.resize(GEN3_SAVE_FILE_SIZE + 16, 0);  // RTC no fim
  SaveReader r{data.size(), mem_read, &data, g_scratch};
  DexStatus st;
  st.clear();
  TEST_ASSERT_NOT_NULL(save_merge_dex(r, st));

  std::vector<uint8_t> small(32 * 1024, 0);  // tamanho de Gen1/2: ainda sem parser
  SaveReader r2{small.size(), mem_read, &small, g_scratch};
  TEST_ASSERT_NULL(save_merge_dex(r2, st));
}

void test_gen3_corrupt_is_rejected() {
  auto data = build_gen3(cfg_dex);
  data[100] ^= 0xFF;  // estraga a seção física 0 (sem recalcular checksum)
  SaveReader r{data.size(), mem_read, &data, g_scratch};
  DexStatus st;
  st.clear();
  TEST_ASSERT_NULL(save_merge_dex(r, st));
  TEST_ASSERT_EQUAL(0, st.count_seen());
}

// ---------- RowPages (Ajustes) ----------

void test_row_pages_single() {
  const uint16_t h[4] = {34, 34, 34, 34};
  RowPages rp = row_pages(h, 4, 208, 170);
  TEST_ASSERT_EQUAL(1, rp.count);
  TEST_ASSERT_FALSE(rp.paged());
  TEST_ASSERT_EQUAL(0, rp.first(0));
  TEST_ASSERT_EQUAL(4, rp.end(0));
  // 6 x 34 = 204 cabe sem rodapé.
  const uint16_t six[6] = {34, 34, 34, 34, 34, 34};
  TEST_ASSERT_EQUAL(1, row_pages(six, 6, 208, 170).count);
  // Vazio: uma página sem linhas.
  RowPages e = row_pages(h, 0, 208, 170);
  TEST_ASSERT_EQUAL(1, e.count);
  TEST_ASSERT_EQUAL(0, e.end(0));
}

void test_row_pages_paged() {
  uint16_t h[7];
  for (auto &v : h) v = 34;
  RowPages rp = row_pages(h, 7, 208, 170);  // 7 x 34 = 238 > 208
  TEST_ASSERT_EQUAL(2, rp.count);
  TEST_ASSERT_TRUE(rp.paged());
  TEST_ASSERT_EQUAL(5, rp.end(0));  // 5 x 34 = 170 com rodapé
  TEST_ASSERT_EQUAL(5, rp.first(1));
  TEST_ASSERT_EQUAL(7, rp.end(1));
  TEST_ASSERT_EQUAL(0, rp.page_of(4));
  TEST_ASSERT_EQUAL(1, rp.page_of(5));
  TEST_ASSERT_EQUAL(1, rp.page_of(6));
}

void test_row_pages_mixed_heights() {
  // Linha alta (52) empurra a seguinte pra próxima página.
  const uint16_t h[6] = {34, 34, 34, 34, 52, 34};
  RowPages rp = row_pages(h, 6, 208, 170);
  TEST_ASSERT_EQUAL(2, rp.count);
  TEST_ASSERT_EQUAL(4, rp.end(0));  // 4 x 34 = 136; + 52 = 188 > 170
  TEST_ASSERT_EQUAL(6, rp.end(1));
  // Linha maior que a página fica sozinha.
  const uint16_t big[3] = {200, 200, 34};
  RowPages b = row_pages(big, 3, 208, 170);
  TEST_ASSERT_EQUAL(3, b.count);
  TEST_ASSERT_EQUAL(1, b.end(0));
  TEST_ASSERT_EQUAL(2, b.end(1));
  TEST_ASSERT_EQUAL(3, b.end(2));
}

void test_showcase_eligible() {
  // (visto, capturado)
  TEST_ASSERT_TRUE(showcase_eligible(ShowcaseSource::Caught, true, true));
  TEST_ASSERT_FALSE(showcase_eligible(ShowcaseSource::Caught, true, false));
  TEST_ASSERT_TRUE(showcase_eligible(ShowcaseSource::Seen, true, false));
  TEST_ASSERT_FALSE(showcase_eligible(ShowcaseSource::Seen, false, false));
  TEST_ASSERT_TRUE(showcase_eligible(ShowcaseSource::All, false, false));
}

// ---------- SaveName ----------

void test_save_name_decodes_and_accepts() {
  char out[SAVE_NAME_MAX + 1];
  TEST_ASSERT_TRUE(save_name_decode("emerald.sav", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("emerald.sav", out);
  TEST_ASSERT_TRUE(save_name_decode("Pokemon%20-%20Emerald%20Version%20(USA).SAV", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Pokemon - Emerald Version (USA).SAV", out);
  TEST_ASSERT_TRUE(save_name_decode("fire+red.srm", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("fire red.srm", out);
}

void test_save_name_rejects() {
  char out[SAVE_NAME_MAX + 1];
  TEST_ASSERT_FALSE(save_name_decode("", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode(".sav", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode(".hidden.sav", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode("%20a.sav", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode("game.gba", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode("..%2Fcydex%2Fspecies.sav", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode("a\\b.sav", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode("bad%zz.sav", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode("bad%.sav", out, sizeof(out)));
  TEST_ASSERT_FALSE(save_name_decode("acento%C3%A9.sav", out, sizeof(out)));
  char longname[80];
  memset(longname, 'a', 61);
  strcpy(longname + 61, ".sav");  // 65 bytes
  TEST_ASSERT_FALSE(save_name_decode(longname, out, sizeof(out)));
  longname[60] = '\0';
  strcpy(longname + 60, ".sav");  // 64 bytes
  TEST_ASSERT_TRUE(save_name_decode(longname, out, sizeof(out)));
  char tiny[8];
  TEST_ASSERT_FALSE(save_name_decode("emerald.sav", tiny, sizeof(tiny)));
}

// ---------- Sun ----------

// UTC de uma data às 12:00 (dias desde 1970 por algoritmo civil).
static int64_t utc_noon(int y, int m, int d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (era * 146097 + doe - 719468) * 86400 + 12 * 3600;
}

static void assert_hhmm(const char *expect, int64_t utc_s, int16_t tz, int tol_min) {
  char got[8];
  sun_format_hhmm(utc_s, tz, got, sizeof(got));
  int e = (expect[0] - '0') * 600 + (expect[1] - '0') * 60 + (expect[3] - '0') * 10 + (expect[4] - '0');
  int g = (got[0] - '0') * 600 + (got[1] - '0') * 60 + (got[3] - '0') * 10 + (got[4] - '0');
  char msg[48];
  snprintf(msg, sizeof(msg), "esperado %s, veio %s", expect, got);
  TEST_ASSERT_INT_WITHIN_MESSAGE(tol_min, e, g, msg);
}

void test_sun_times_known_cities() {
  // São Paulo (−03): inverno 06:47–17:28, verão 05:16–18:52 (NOAA completo).
  SunDay d = sun_day(utc_noon(2026, 6, 21), -23.5505, -46.6333);
  TEST_ASSERT_EQUAL(SunKind::Normal, d.kind);
  assert_hhmm("06:47", d.rise, -180, 2);
  assert_hhmm("17:28", d.set, -180, 2);
  d = sun_day(utc_noon(2026, 12, 21), -23.5505, -46.6333);
  assert_hhmm("05:16", d.rise, -180, 2);
  assert_hhmm("18:52", d.set, -180, 2);
  // Lisboa (+01 no verão): 06:11–21:04.
  d = sun_day(utc_noon(2026, 6, 21), 38.7223, -9.1393);
  assert_hhmm("06:11", d.rise, 60, 2);
  assert_hhmm("21:04", d.set, 60, 2);
}

void test_sun_is_night_and_day_boundary() {
  int64_t noon = utc_noon(2026, 6, 21);
  // São Paulo: 15 h UTC = 12 h local (dia); 01 h UTC do dia seguinte = 22 h local.
  TEST_ASSERT_FALSE(sun_is_night(noon + 3 * 3600, -23.55, -46.63));
  TEST_ASSERT_TRUE(sun_is_night(noon + 13 * 3600, -23.55, -46.63));
  // Madrugada local (03 h UTC = 00 h local).
  TEST_ASSERT_TRUE(sun_is_night(noon - 9 * 3600, -23.55, -46.63));
  // Tóquio (+09): 12 h UTC = 21 h local, noite; 03 h UTC = 12 h local, dia.
  TEST_ASSERT_TRUE(sun_is_night(noon, 35.68, 139.69));
  TEST_ASSERT_FALSE(sun_is_night(noon - 9 * 3600, 35.68, 139.69));
  // O "dia" do pôr do sol é o do dia solar local, mesmo cruzando 00 h UTC.
  SunDay a = sun_day(noon + 13 * 3600, -23.55, -46.63);
  SunDay b = sun_day(noon, -23.55, -46.63);
  TEST_ASSERT_EQUAL_INT64(b.set, a.set);
}

void test_sun_polar() {
  TEST_ASSERT_EQUAL(SunKind::PolarDay, sun_day(utc_noon(2026, 6, 21), 69.65, 18.96).kind);
  TEST_ASSERT_EQUAL(SunKind::PolarNight, sun_day(utc_noon(2026, 12, 21), 69.65, 18.96).kind);
  TEST_ASSERT_FALSE(sun_is_night(utc_noon(2026, 6, 21), 69.65, 18.96));
  TEST_ASSERT_TRUE(sun_is_night(utc_noon(2026, 12, 21), 69.65, 18.96));
}

void test_sun_format_wraps() {
  char out[8];
  sun_format_hhmm(0, -180, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("21:00", out);
  sun_format_hhmm(23 * 3600 + 30 * 60, 60, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("00:30", out);
}

void test_sun_parse_location() {
  float lat = 0, lon = 0;
  int16_t tz = 0;
  TEST_ASSERT_NULL(sun_parse_location("lat=-23.5505&lon=-46.6333&tz=-180", &lat, &lon, &tz));
  TEST_ASSERT_FLOAT_WITHIN(1e-4, -23.5505f, lat);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, -46.6333f, lon);
  TEST_ASSERT_EQUAL(-180, tz);
  TEST_ASSERT_NULL(sun_parse_location("tz=540&lon=139.69&lat=35.68", &lat, &lon, &tz));
  TEST_ASSERT_EQUAL_STRING("lat", sun_parse_location("", &lat, &lon, &tz));
  TEST_ASSERT_EQUAL_STRING("lat", sun_parse_location("lat=91&lon=0&tz=0", &lat, &lon, &tz));
  TEST_ASSERT_EQUAL_STRING("lat", sun_parse_location("lat=abc&lon=0&tz=0", &lat, &lon, &tz));
  TEST_ASSERT_EQUAL_STRING("lon", sun_parse_location("lat=0&lon=180.5&tz=0", &lat, &lon, &tz));
  TEST_ASSERT_EQUAL_STRING("lon", sun_parse_location("lat=0&tz=0", &lat, &lon, &tz));
  TEST_ASSERT_EQUAL_STRING("tz", sun_parse_location("lat=0&lon=0&tz=900", &lat, &lon, &tz));
  TEST_ASSERT_EQUAL_STRING("tz", sun_parse_location("lat=0&lon=0&tz=1.5", &lat, &lon, &tz));
  TEST_ASSERT_EQUAL_STRING("tz", sun_parse_location("lat=0&lon=0", &lat, &lon, &tz));
}

// ---------- WakeLease (POST /wake?ttl) ----------

void test_wake_lease_renew_and_expire() {
  WakeLease l;
  TEST_ASSERT_FALSE(l.active());
  TEST_ASSERT_FALSE(l.expired(1000));
  // Primeiro: sem prazo valendo, conta como /wake de verdade.
  TEST_ASSERT_TRUE(l.renew(1000, 90));
  TEST_ASSERT_EQUAL_UINT32(90000, l.left_ms(1000));
  // Renovação: só estende.
  TEST_ASSERT_FALSE(l.renew(31000, 90));
  TEST_ASSERT_FALSE(l.expired(120999));
  TEST_ASSERT_EQUAL_UINT32(1, l.left_ms(120999));
  TEST_ASSERT_TRUE(l.expired(121000));
  // Só uma vez.
  TEST_ASSERT_FALSE(l.expired(130000));
  TEST_ASSERT_FALSE(l.active());
  TEST_ASSERT_EQUAL_UINT32(0, l.left_ms(130000));
  // Depois de vencer, o próximo volta a ser um /wake de verdade.
  TEST_ASSERT_TRUE(l.renew(140000, 90));
  l.clear();
  TEST_ASSERT_FALSE(l.expired(1000000));
}

void test_wake_lease_millis_wrap() {
  WakeLease l;
  uint32_t start = 0xFFFFFFFFu - 10000;  // 10 s antes do millis() voltar a 0
  l.renew(start, 90);
  TEST_ASSERT_FALSE(l.expired(start + 89999));  // já deu a volta
  TEST_ASSERT_TRUE(l.expired(start + 90000));
}

void test_wake_lease_parse() {
  uint32_t ttl = 7;
  TEST_ASSERT_NULL(wake_lease_parse("", &ttl));
  TEST_ASSERT_EQUAL_UINT32(0, ttl);
  TEST_ASSERT_NULL(wake_lease_parse(nullptr, &ttl));
  TEST_ASSERT_NULL(wake_lease_parse("ttl=90", &ttl));
  TEST_ASSERT_EQUAL_UINT32(90, ttl);
  TEST_ASSERT_NULL(wake_lease_parse("x=1&ttl=3600", &ttl));
  TEST_ASSERT_EQUAL_UINT32(3600, ttl);
  TEST_ASSERT_NULL(wake_lease_parse("ttl=0", &ttl));
  TEST_ASSERT_EQUAL_UINT32(0, ttl);
  TEST_ASSERT_NULL(wake_lease_parse("other=1", &ttl));
  TEST_ASSERT_EQUAL_UINT32(0, ttl);
  TEST_ASSERT_EQUAL_STRING("ttl", wake_lease_parse("ttl=3601", &ttl));
  TEST_ASSERT_EQUAL_STRING("ttl", wake_lease_parse("ttl=-5", &ttl));
  TEST_ASSERT_EQUAL_STRING("ttl", wake_lease_parse("ttl=9.5", &ttl));
  TEST_ASSERT_EQUAL_STRING("ttl", wake_lease_parse("ttl=", &ttl));
  TEST_ASSERT_EQUAL_STRING("ttl", wake_lease_parse("ttl=99999999999", &ttl));
}

// ---------- SettingsPatch (PUT /settings) ----------

void test_settings_patch_reads_fields() {
  SettingsPatch p;
  TEST_ASSERT_NULL(settings_patch_parse("bright=70&bright_n=5&off_min=0&sound=1", &p));
  TEST_ASSERT_TRUE(p.has_bright && p.has_bright_n && p.has_off && p.has_sound);
  TEST_ASSERT_EQUAL(70, p.bright);
  TEST_ASSERT_EQUAL(5, p.bright_n);
  TEST_ASSERT_EQUAL(0, p.off_min);
  TEST_ASSERT_TRUE(p.sound);
  TEST_ASSERT_FALSE(p.has_grid || p.has_lang || p.has_sc_int);

  TEST_ASSERT_NULL(settings_patch_parse("sc_int=900&sc_rand=false&sc_src=3&sc_boot=true", &p));
  TEST_ASSERT_EQUAL(900, p.sc_int);
  TEST_ASSERT_FALSE(p.sc_rand);
  TEST_ASSERT_EQUAL(3, p.sc_src);
  TEST_ASSERT_TRUE(p.sc_boot);
  TEST_ASSERT_FALSE(p.has_bright);  // zera entre chamadas

  TEST_ASSERT_NULL(settings_patch_parse("lang=en&grid=0", &p));
  TEST_ASSERT_EQUAL_STRING("en", p.lang);
  TEST_ASSERT_TRUE(p.has_grid);
  TEST_ASSERT_FALSE(p.grid);
}

void test_settings_patch_rejects() {
  SettingsPatch p;
  TEST_ASSERT_EQUAL_STRING("query", settings_patch_parse("", &p));
  TEST_ASSERT_EQUAL_STRING("query", settings_patch_parse(nullptr, &p));
  TEST_ASSERT_EQUAL_STRING("query", settings_patch_parse("bright", &p));
  TEST_ASSERT_EQUAL_STRING("query", settings_patch_parse("=5", &p));
  TEST_ASSERT_EQUAL_STRING("bright", settings_patch_parse("bright=9", &p));
  TEST_ASSERT_EQUAL_STRING("bright", settings_patch_parse("bright=101", &p));
  TEST_ASSERT_EQUAL_STRING("bright", settings_patch_parse("bright=", &p));
  TEST_ASSERT_EQUAL_STRING("bright_n", settings_patch_parse("bright_n=4", &p));
  TEST_ASSERT_EQUAL_STRING("off_min", settings_patch_parse("off_min=3", &p));
  TEST_ASSERT_EQUAL_STRING("sc_int", settings_patch_parse("sc_int=45", &p));
  TEST_ASSERT_EQUAL_STRING("sc_src", settings_patch_parse("sc_src=4", &p));
  TEST_ASSERT_EQUAL_STRING("sound", settings_patch_parse("sound=yes", &p));
  TEST_ASSERT_EQUAL_STRING("lang", settings_patch_parse("lang=xx", &p));
  TEST_ASSERT_EQUAL_STRING("lang", settings_patch_parse("lang=pt", &p));
  TEST_ASSERT_EQUAL_STRING("bogus", settings_patch_parse("bogus=1", &p));
  TEST_ASSERT_EQUAL_STRING("bright", settings_patch_parse("bright=999999999999", &p));
  // Tudo ou nada: o válido antes do inválido não vale.
  TEST_ASSERT_EQUAL_STRING("sound", settings_patch_parse("bright=50&sound=2", &p));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_generation_boundaries);
  RUN_TEST(test_status_caught_implies_seen);
  RUN_TEST(test_status_merge);
  RUN_TEST(test_species_record_roundtrip);
  RUN_TEST(test_species_record_rejects_bad_type_and_unterminated_name);
  RUN_TEST(test_species_header);
  RUN_TEST(test_sprite_file_header_and_offsets);
  RUN_TEST(test_signature_is_order_independent);
  RUN_TEST(test_cache_roundtrip_and_invalidation);
  RUN_TEST(test_pager_basics);
  RUN_TEST(test_hold_repeat_accelerates);
  RUN_TEST(test_catalog_status_filters);
  RUN_TEST(test_catalog_type_filters);
  RUN_TEST(test_catalog_index_at_or_after);
  RUN_TEST(test_catalog_neighbors);
  RUN_TEST(test_format_tenths);
  RUN_TEST(test_search_fold);
  RUN_TEST(test_search_matches);
  RUN_TEST(test_catalog_text_and_pick_type);
  RUN_TEST(test_option_step);
  RUN_TEST(test_showcase_next);
  RUN_TEST(test_showcase_pick);
  RUN_TEST(test_showcase_shift_and_cycle);
  RUN_TEST(test_showcase_crop);
  RUN_TEST(test_showcase_eligible);
  RUN_TEST(test_row_pages_single);
  RUN_TEST(test_row_pages_paged);
  RUN_TEST(test_row_pages_mixed_heights);
  RUN_TEST(test_number_input);
  RUN_TEST(test_gen3_merge_dex);
  RUN_TEST(test_gen3_tolerates_trailing_bytes_and_rejects_other_sizes);
  RUN_TEST(test_gen3_corrupt_is_rejected);
  RUN_TEST(test_save_name_decodes_and_accepts);
  RUN_TEST(test_save_name_rejects);
  RUN_TEST(test_sun_times_known_cities);
  RUN_TEST(test_sun_is_night_and_day_boundary);
  RUN_TEST(test_sun_polar);
  RUN_TEST(test_sun_format_wraps);
  RUN_TEST(test_sun_parse_location);
  RUN_TEST(test_wake_lease_renew_and_expire);
  RUN_TEST(test_wake_lease_millis_wrap);
  RUN_TEST(test_wake_lease_parse);
  RUN_TEST(test_settings_patch_reads_fields);
  RUN_TEST(test_settings_patch_rejects);
  return UNITY_END();
}
