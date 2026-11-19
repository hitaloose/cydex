#include <unity.h>

#include <cstring>
#include <vector>

#include "saves/gen3/Gen3PokedexInfo.h"
#include "saves/gen3/Gen3SaveParser.h"
#include "saves/gen3/Gen3TextDecoder.h"
#include "saves/gen3/Gen3TrainerInfo.h"
#include "utils/ByteUtils.h"

// ---------------------------------------------------------------------------
// Helpers pra montar um save Gen3 sintético válido (checksum, contador, IDs
// de seção) sem precisar de um dump real de jogo.
// ---------------------------------------------------------------------------

static void write_section_footer(uint8_t *section, uint16_t id, uint32_t counter) {
  section[GEN3_SECTOR_FOOTER_ID_OFFSET] = (uint8_t)(id & 0xFF);
  section[GEN3_SECTOR_FOOTER_ID_OFFSET + 1] = (uint8_t)(id >> 8);

  uint16_t checksum = gen3_checksum32(section, GEN3_SECTOR_DATA_SIZE);
  section[GEN3_SECTOR_FOOTER_CHECKSUM_OFFSET] = (uint8_t)(checksum & 0xFF);
  section[GEN3_SECTOR_FOOTER_CHECKSUM_OFFSET + 1] = (uint8_t)(checksum >> 8);

  section[GEN3_SECTOR_FOOTER_COUNTER_OFFSET] = (uint8_t)(counter & 0xFF);
  section[GEN3_SECTOR_FOOTER_COUNTER_OFFSET + 1] = (uint8_t)((counter >> 8) & 0xFF);
  section[GEN3_SECTOR_FOOTER_COUNTER_OFFSET + 2] = (uint8_t)((counter >> 16) & 0xFF);
  section[GEN3_SECTOR_FOOTER_COUNTER_OFFSET + 3] = (uint8_t)((counter >> 24) & 0xFF);
}

// Monta um save de 128KB com o slot 0 totalmente válido (14 seções, IDs
// 0..13, checksum correto) e o slot 1 vazio (fica de fora da seleção de
// slot ativo). `configure_small`/`configure_large` recebem os blocos ainda
// não fatiados em seções, pra o teste poder escrever campos específicos
// (nome do treinador, badges etc.) antes do checksum ser calculado.
template <typename ConfigureSmall, typename ConfigureLarge>
static std::vector<uint8_t> build_synthetic_save(ConfigureSmall configure_small,
                                                   ConfigureLarge configure_large,
                                                   uint32_t counter = 1) {
  std::vector<uint8_t> data(GEN3_SAVE_FILE_SIZE, 0);

  uint8_t small[GEN3_SECTOR_DATA_SIZE] = {0};
  configure_small(small);

  uint8_t large[GEN3_LARGE_BLOCK_SECTION_COUNT][GEN3_SECTOR_DATA_SIZE] = {{0}};
  configure_large(large);

  // Seção 0 = small block, seções 1-4 = large block, seções 5-13 = storage
  // (zeradas — não usadas pelo M7).
  for (int i = 0; i < GEN3_SECTIONS_PER_SLOT; i++) {
    uint8_t *section = data.data() + (i * GEN3_SECTOR_SIZE);
    if (i == 0) {
      memcpy(section, small, GEN3_SECTOR_DATA_SIZE);
    } else if (i >= 1 && i <= GEN3_LARGE_BLOCK_SECTION_COUNT) {
      memcpy(section, large[i - 1], GEN3_SECTOR_DATA_SIZE);
    }
    write_section_footer(section, (uint16_t)i, counter);
  }

  return data;
}

// ---------------------------------------------------------------------------
// Checksum: valores calculados à mão (não só round-trip com nosso próprio
// código), conferindo o algoritmo em si (soma de palavras de 32 bits +
// dobra do carry uma vez), confirmado no código-fonte do PKHeX.
// ---------------------------------------------------------------------------

void test_checksum_all_zero_is_zero() {
  uint8_t buf[GEN3_SECTOR_DATA_SIZE] = {0};
  TEST_ASSERT_EQUAL_UINT16(0, gen3_checksum32(buf, GEN3_SECTOR_DATA_SIZE));
}

void test_checksum_known_value() {
  uint8_t buf[GEN3_SECTOR_DATA_SIZE] = {0};
  buf[0] = 0xFF;
  buf[1] = 0xFF;
  buf[2] = 0xFF;
  buf[3] = 0xFF;
  // soma = 0xFFFFFFFF; + (soma >> 16) = 0xFFFFFFFF + 0x0000FFFF, estoura 32
  // bits e sobra 0x0000FFFE; corta pra 16 bits = 0xFFFE.
  TEST_ASSERT_EQUAL_HEX16(0xFFFE, gen3_checksum32(buf, GEN3_SECTOR_DATA_SIZE));
}

// ---------------------------------------------------------------------------
// Decodificador de texto Gen3 -> UTF-8.
// ---------------------------------------------------------------------------

void test_text_decoder_basic_name() {
  // A=0xBB, S=0xCD (maiúsculo), H=0xC2, terminador 0xFF
  uint8_t encoded[] = {0xBB, 0xCD, 0xC2, 0xFF, 0xFF, 0xFF, 0xFF};
  char out[16];
  gen3_text_decode(encoded, sizeof(encoded), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("ASH", out);
}

void test_text_decoder_stops_at_terminator() {
  uint8_t encoded[] = {0xBB, 0xBB, 0xFF, 0xBB, 0xBB};  // "AA" + lixo depois do terminador
  char out[16];
  gen3_text_decode(encoded, sizeof(encoded), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("AA", out);
}

// ---------------------------------------------------------------------------
// Parser completo: slot ativo, versão, trainer info, pokedex.
// ---------------------------------------------------------------------------

void test_parser_rejects_undersized_buffer() {
  uint8_t tiny[16] = {0};
  Gen3ParsedSave save = gen3_parse_save(tiny, sizeof(tiny));
  TEST_ASSERT_FALSE(save.valid);
}

void test_parser_picks_slot_with_all_sections_and_valid_checksum() {
  auto data = build_synthetic_save(
      [](uint8_t *small) {
        small[GEN3_SMALL_VERSION_PROBE_OFFSET] = 0;  // RS
      },
      [](uint8_t large[GEN3_LARGE_BLOCK_SECTION_COUNT][GEN3_SECTOR_DATA_SIZE]) { (void)large; });

  Gen3ParsedSave save = gen3_parse_save(data.data(), data.size());
  TEST_ASSERT_TRUE(save.valid);
}

void test_parser_detects_frlg() {
  auto data = build_synthetic_save(
      [](uint8_t *small) {
        small[GEN3_SMALL_VERSION_PROBE_OFFSET] = 1;  // FRLG (valor fixo)
      },
      [](uint8_t large[GEN3_LARGE_BLOCK_SECTION_COUNT][GEN3_SECTOR_DATA_SIZE]) { (void)large; });

  Gen3ParsedSave save = gen3_parse_save(data.data(), data.size());
  TEST_ASSERT_TRUE(save.valid);
  TEST_ASSERT_EQUAL_INT((int)Gen3GameVersion::FireRedLeafGreen, (int)save.version);
}

void test_parser_detects_emerald() {
  auto data = build_synthetic_save(
      [](uint8_t *small) {
        small[GEN3_SMALL_VERSION_PROBE_OFFSET] = 0x42;  // nem 0 nem 1
        small[GEN3_SMALL_EMERALD_ONLY_RANGE_START] = 0x01;  // só a Emerald usa essa área
      },
      [](uint8_t large[GEN3_LARGE_BLOCK_SECTION_COUNT][GEN3_SECTOR_DATA_SIZE]) { (void)large; });

  Gen3ParsedSave save = gen3_parse_save(data.data(), data.size());
  TEST_ASSERT_TRUE(save.valid);
  TEST_ASSERT_EQUAL_INT((int)Gen3GameVersion::Emerald, (int)save.version);
}

void test_parser_extracts_trainer_info() {
  auto data = build_synthetic_save(
      [](uint8_t *small) {
        small[GEN3_SMALL_VERSION_PROBE_OFFSET] = 0;  // RS
        // Nome "RED" (R=0xCC, E=0xBF, D=0xBE)
        small[GEN3_SMALL_OT_NAME_OFFSET + 0] = 0xCC;
        small[GEN3_SMALL_OT_NAME_OFFSET + 1] = 0xBF;
        small[GEN3_SMALL_OT_NAME_OFFSET + 2] = 0xBE;
        small[GEN3_SMALL_OT_NAME_OFFSET + 3] = 0xFF;
        // TID=12345, SID=6789
        small[GEN3_SMALL_TID_OFFSET] = 0x39;
        small[GEN3_SMALL_TID_OFFSET + 1] = 0x30;
        small[GEN3_SMALL_SID_OFFSET] = 0x85;
        small[GEN3_SMALL_SID_OFFSET + 1] = 0x1A;
        // 34h12m56s jogadas
        small[GEN3_SMALL_PLAYED_HOURS_OFFSET] = 34;
        small[GEN3_SMALL_PLAYED_HOURS_OFFSET + 1] = 0;
        small[GEN3_SMALL_PLAYED_MINUTES_OFFSET] = 12;
        small[GEN3_SMALL_PLAYED_SECONDS_OFFSET] = 56;
      },
      [](uint8_t large[GEN3_LARGE_BLOCK_SECTION_COUNT][GEN3_SECTOR_DATA_SIZE]) {
        // RS: event flags em 0x1220, badges a partir da flag 0x807. Liga as
        // 3 primeiras badges (bits 0,1,2) — a seção certa dentro do Large
        // block é (base + flag/8) / SECTOR_DATA_SIZE.
        int base = GEN3_BADGE_LOCATION_RS.event_flag_base;
        int start = GEN3_BADGE_LOCATION_RS.badge_flag_start;
        for (int i = 0; i < 3; i++) {
          int flag = start + i;
          int abs_offset = base + (flag >> 3);
          int section_index = abs_offset / GEN3_SECTOR_DATA_SIZE;
          int offset_in_section = abs_offset % GEN3_SECTOR_DATA_SIZE;
          large[section_index][offset_in_section] |= (uint8_t)(1 << (flag & 7));
        }
      });

  Gen3ParsedSave save = gen3_parse_save(data.data(), data.size());
  TEST_ASSERT_TRUE(save.valid);

  Trainer trainer = gen3_extract_trainer_info(save);
  TEST_ASSERT_EQUAL_STRING("RED", trainer.name);
  TEST_ASSERT_EQUAL_UINT16(12345, trainer.tid);
  TEST_ASSERT_EQUAL_UINT16(6789, trainer.sid);
  TEST_ASSERT_EQUAL_UINT16(34, trainer.played_hours);
  TEST_ASSERT_EQUAL_UINT8(12, trainer.played_minutes);
  TEST_ASSERT_EQUAL_UINT8(56, trainer.played_seconds);
  TEST_ASSERT_EQUAL_HEX8(0b00000111, trainer.badges);  // 3 badges ligadas
}

void test_parser_pokedex_seen_caught() {
  auto data = build_synthetic_save(
      [](uint8_t *small) {
        small[GEN3_SMALL_VERSION_PROBE_OFFSET] = 0;  // RS
        // Bulbasaur (#1, bit 0) capturado; Charmander (#4, bit 3) só visto.
        small[GEN3_SMALL_POKEDEX_CAUGHT_OFFSET] |= 0b00000001;
        small[GEN3_SMALL_POKEDEX_SEEN_OFFSET] |= 0b00001001;  // bits 0 e 3
      },
      [](uint8_t large[GEN3_LARGE_BLOCK_SECTION_COUNT][GEN3_SECTOR_DATA_SIZE]) { (void)large; });

  Gen3ParsedSave save = gen3_parse_save(data.data(), data.size());
  TEST_ASSERT_TRUE(save.valid);

  TEST_ASSERT_TRUE(gen3_pokedex_is_caught(save, 1));
  TEST_ASSERT_FALSE(gen3_pokedex_is_caught(save, 4));
  TEST_ASSERT_TRUE(gen3_pokedex_is_seen(save, 1));
  TEST_ASSERT_TRUE(gen3_pokedex_is_seen(save, 4));
  TEST_ASSERT_FALSE(gen3_pokedex_is_seen(save, 2));

  TEST_ASSERT_EQUAL_UINT16(1, gen3_pokedex_count_caught(save));
  TEST_ASSERT_EQUAL_UINT16(2, gen3_pokedex_count_seen(save));
}

void test_parser_pokedex_copy_caught_bitmask() {
  auto data = build_synthetic_save(
      [](uint8_t *small) {
        small[GEN3_SMALL_VERSION_PROBE_OFFSET] = 0;  // RS
        // Bulbasaur (#1, bit 0) e Deoxys (#386, último bit válido) capturados.
        small[GEN3_SMALL_POKEDEX_CAUGHT_OFFSET] |= 0b00000001;
        int last_bit = GEN3_NATIONAL_DEX_SPECIES_COUNT - 1;  // 385
        small[GEN3_SMALL_POKEDEX_CAUGHT_OFFSET + (last_bit >> 3)] |= (uint8_t)(1 << (last_bit & 7));
      },
      [](uint8_t large[GEN3_LARGE_BLOCK_SECTION_COUNT][GEN3_SECTOR_DATA_SIZE]) { (void)large; });

  Gen3ParsedSave save = gen3_parse_save(data.data(), data.size());
  TEST_ASSERT_TRUE(save.valid);

  uint8_t bitmask[GEN3_POKEDEX_BITMASK_BYTES] = {0};
  gen3_pokedex_copy_caught_bitmask(save, bitmask);

  for (uint16_t species = 1; species <= GEN3_NATIONAL_DEX_SPECIES_COUNT; species++) {
    int bit = species - 1;
    bool bitmask_says_caught = (bitmask[bit >> 3] >> (bit & 7)) & 1;
    TEST_ASSERT_EQUAL(gen3_pokedex_is_caught(save, species), bitmask_says_caught);
  }
  TEST_ASSERT_TRUE(gen3_pokedex_is_caught(save, 1));
  TEST_ASSERT_TRUE(gen3_pokedex_is_caught(save, GEN3_NATIONAL_DEX_SPECIES_COUNT));
}

void test_parser_invalid_checksum_is_rejected() {
  auto data = build_synthetic_save(
      [](uint8_t *small) { small[GEN3_SMALL_VERSION_PROBE_OFFSET] = 0; },
      [](uint8_t large[GEN3_LARGE_BLOCK_SECTION_COUNT][GEN3_SECTOR_DATA_SIZE]) { (void)large; });

  // Corrompe um byte de dado da seção 0 sem recalcular o checksum.
  data[10] ^= 0xFF;

  Gen3ParsedSave save = gen3_parse_save(data.data(), data.size());
  TEST_ASSERT_FALSE(save.valid);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();
  RUN_TEST(test_checksum_all_zero_is_zero);
  RUN_TEST(test_checksum_known_value);
  RUN_TEST(test_text_decoder_basic_name);
  RUN_TEST(test_text_decoder_stops_at_terminator);
  RUN_TEST(test_parser_rejects_undersized_buffer);
  RUN_TEST(test_parser_picks_slot_with_all_sections_and_valid_checksum);
  RUN_TEST(test_parser_detects_frlg);
  RUN_TEST(test_parser_detects_emerald);
  RUN_TEST(test_parser_extracts_trainer_info);
  RUN_TEST(test_parser_pokedex_seen_caught);
  RUN_TEST(test_parser_pokedex_copy_caught_bitmask);
  RUN_TEST(test_parser_invalid_checksum_is_rejected);
  return UNITY_END();
}
