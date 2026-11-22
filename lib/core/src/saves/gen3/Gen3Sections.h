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

// ---- Campos usados pelos Saves (PKHeX: Saves/Blocks/Gen3/SaveBlock3*) ----
constexpr int GEN3_SMALL_GENDER_OFFSET = 0x08;  // u8: 0 masculino, 1 feminino
// Chave que cifra dinheiro e game stats (XOR u32). Ruby/Sapphire não têm (0).
constexpr int GEN3_SMALL_SECURITY_KEY_EMERALD = 0x0AC;
constexpr int GEN3_SMALL_SECURITY_KEY_FRLG = 0xF20;

// Offsets dentro do Large block remontado (seções 1..4).
struct Gen3LargeLayout {
  int party_count;  // u8
  int party;        // 6 × 100 B
  int money;        // u32 (XOR chave)
  int records;      // game stats: u32 cada (XOR chave)
};
constexpr Gen3LargeLayout GEN3_LARGE_RS = {0x234, 0x238, 0x490, 0x1540};
constexpr Gen3LargeLayout GEN3_LARGE_EMERALD = {0x234, 0x238, 0x490, 0x159C};
constexpr Gen3LargeLayout GEN3_LARGE_FRLG = {0x034, 0x038, 0x290, 0x1200};
// Game stat 1 = tempo de jogo na 1ª vitória (h << 16 | min << 8 | s).
constexpr int GEN3_RECORD_FIRST_HOF_PLAY_TIME = 1;

// Storage (PC): seções 5..13 remontadas (9 × 0xF80 = 0x83D0 B).
constexpr int GEN3_STORAGE_FIRST_SECTION_ID = 5;
constexpr int GEN3_BOX_COUNT = 14;
constexpr int GEN3_BOX_SLOTS = 30;
constexpr int GEN3_STORAGE_BOXES_OFFSET = 0x04;  // 14 × 30 × 80 B
constexpr int GEN3_STORAGE_BOX_NAMES_OFFSET = 0x8344;
constexpr int GEN3_BOX_NAME_LENGTH = 9;  // 8 + terminador

// Hall da Fama: fora dos slots, nos setores físicos 28 e 29. Cada um usa os
// primeiros 0xF80 B; juntos guardam 50 times de 6 × 20 B (o 34º atravessa
// a divisa). O checksum dos setores extras fica em 0xFF4 (não 0xFF6), e um
// setor todo 0x00 ou 0xFF conta como vazio. O jogo grava cada time novo na
// 1ª vaga livre (com 50, desloca tudo): o índice 0 é o mais antigo.
constexpr long GEN3_HOF_SECTOR_A = 0x1C000;
constexpr long GEN3_HOF_SECTOR_B = 0x1D000;
constexpr int GEN3_EXTRA_CHECKSUM_OFFSET = 0xFF4;
constexpr int GEN3_HOF_TEAMS = 50;
constexpr int GEN3_HOF_MON_SIZE = 0x14;  // u32 ID, u32 PID, u16 espécie:9|nível:7, apelido 10 B
constexpr int GEN3_HOF_TEAM_SIZE = 6 * GEN3_HOF_MON_SIZE;
