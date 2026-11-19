#pragma once

// Constantes do formato de save Gen3 (Ruby/Sapphire/Emerald/FireRed/
// LeafGreen), confirmadas no código-fonte do PKHeX (PKHeX.Core/Saves/SAV3.cs
// e PKHeX.Core/Saves/Blocks/Gen3/*.cs) — usado só como referência de layout
// binário, nunca executado no ESP32 (ver CLAUDE.md).

// ---- Estrutura física do arquivo de save ----
constexpr int GEN3_SECTOR_SIZE = 0x1000;       // 4096 — tamanho bruto de cada seção no arquivo
constexpr int GEN3_SECTOR_DATA_SIZE = 0xF80;   // 3968 — bytes de dado + usados no checksum
constexpr int GEN3_SECTOR_FOOTER_ID_OFFSET = 0xFF4;        // u16 LE
constexpr int GEN3_SECTOR_FOOTER_CHECKSUM_OFFSET = 0xFF6;  // u16 LE
constexpr int GEN3_SECTOR_FOOTER_COUNTER_OFFSET = 0xFFC;   // u32 LE

constexpr int GEN3_SECTIONS_PER_SLOT = 14;
constexpr int GEN3_SLOT_SIZE = GEN3_SECTIONS_PER_SLOT * GEN3_SECTOR_SIZE;  // 0xE000
constexpr long GEN3_SAVE_FILE_SIZE = 0x20000;  // 128KB — tamanho mínimo esperado de um dump

// IDs de seção: 0 = Small (trainer/opções/pokedex), 1-4 = Large (times/party/
// eventos), 5-13 = Storage (PC boxes, fora do escopo da v1).
constexpr int GEN3_SMALL_BLOCK_SECTION_ID = 0;
constexpr int GEN3_LARGE_BLOCK_FIRST_SECTION_ID = 1;
constexpr int GEN3_LARGE_BLOCK_SECTION_COUNT = 4;

constexpr int GEN3_SMALL_BLOCK_SIZE = GEN3_SECTOR_DATA_SIZE;                        // 3968
constexpr int GEN3_LARGE_BLOCK_SIZE = GEN3_LARGE_BLOCK_SECTION_COUNT * GEN3_SECTOR_DATA_SIZE;  // 15872

// ---- Offsets dentro do Small block já remontado (0..3967) ----
constexpr int GEN3_SMALL_OT_NAME_OFFSET = 0x00;
constexpr int GEN3_SMALL_OT_NAME_LENGTH = 7;  // + 1 byte terminador (0xFF)
constexpr int GEN3_SMALL_TID_OFFSET = 0x0A;
constexpr int GEN3_SMALL_SID_OFFSET = 0x0C;
constexpr int GEN3_SMALL_PLAYED_HOURS_OFFSET = 0x0E;    // u16 LE
constexpr int GEN3_SMALL_PLAYED_MINUTES_OFFSET = 0x10;  // u8
constexpr int GEN3_SMALL_PLAYED_SECONDS_OFFSET = 0x11;  // u8

// Heurística de detecção de versão (igual ao PKHeX SaveUtil.GetVersionG3SAV):
// lê u32 LE em 0xAC do Small block. ==1 → FRLG (valor fixo). ==0 → RS (sem
// dado de Battle Tower). Outro valor → checar se há algo além de zero em
// 0x890..0xF2C (área que só a Emerald usa); se sim, Emerald, senão RS.
constexpr int GEN3_SMALL_VERSION_PROBE_OFFSET = 0xAC;
constexpr int GEN3_SMALL_EMERALD_ONLY_RANGE_START = 0x890;
constexpr int GEN3_SMALL_EMERALD_ONLY_RANGE_END = 0xF2C;

constexpr int GEN3_SMALL_POKEDEX_BASE = 0x18;
constexpr int GEN3_SMALL_POKEDEX_CAUGHT_OFFSET = GEN3_SMALL_POKEDEX_BASE + 0x10;  // 0x28
constexpr int GEN3_SMALL_POKEDEX_SEEN_OFFSET = GEN3_SMALL_POKEDEX_BASE + 0x44;    // 0x5C
constexpr int GEN3_NATIONAL_DEX_SPECIES_COUNT = 386;  // Bulbasaur..Deoxys (dex nacional até a gen3)
// Tamanho em bytes do bitfield seen/caught (1 bit por espécie, ceil(386/8)).
constexpr int GEN3_POKEDEX_BITMASK_BYTES = (GEN3_NATIONAL_DEX_SPECIES_COUNT + 7) / 8;

// ---- Badges: localização dentro do Large block, por versão ----
// Nas ROMs, badges são flags de evento comuns (não um campo dedicado), e a
// numeração dessas flags difere por jogo — por isso os offsets abaixo variam.
struct Gen3BadgeLocation {
  int event_flag_base;   // offset do array de event flags dentro do Large block
  int badge_flag_start;  // número da primeira das 8 flags de badge
};

constexpr Gen3BadgeLocation GEN3_BADGE_LOCATION_RS = {0x1220, 0x807};
constexpr Gen3BadgeLocation GEN3_BADGE_LOCATION_EMERALD = {0x1270, 0x867};
constexpr Gen3BadgeLocation GEN3_BADGE_LOCATION_FRLG = {0x0EE0, 0x820};
