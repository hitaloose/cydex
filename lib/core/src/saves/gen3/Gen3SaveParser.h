#pragma once

#include <cstddef>
#include <cstdint>

#include "Gen3Sections.h"
#include "models/Gen3GameVersion.h"

// Save já parseado: slot ativo escolhido, checksums validados, Small/Large
// block remontados (na ordem lógica, não na ordem física em que as seções
// apareciam no arquivo) e versão do jogo detectada. Storage (PC boxes) não é
// remontado — fora do escopo da v1 (Gen3BoxParser é stub).
struct Gen3ParsedSave {
  bool valid;
  Gen3GameVersion version;
  uint8_t small_block[GEN3_SMALL_BLOCK_SIZE];
  uint8_t large_block[GEN3_LARGE_BLOCK_SIZE];
};

// Devolve os GEN3_SECTOR_SIZE (4096) bytes da seção física
// `physical_section_index` (0..27 — 14 seções por slot × 2 slots, na ordem
// em que aparecem no arquivo) em `out`. Retorna false se não conseguir ler
// (arquivo menor que o esperado, erro de I/O) — o parser trata a seção como
// ausente/corrompida sem travar, igual a uma seção com ID/checksum inválido.
using Gen3SectionReader = bool (*)(int physical_section_index, uint8_t *out, void *user_data);

// Faz o parsing completo lendo uma seção de 4KB por vez via `read_section`,
// em vez de exigir os ~112KB dos dois slots num buffer contíguo — a placa
// alvo não tem PSRAM e o heap fragmenta antes de conseguir um bloco
// contíguo desse tamanho, mesmo com heap livre total bem maior (achado em
// campo no M8; ver CLAUDE.md). Escolhe o slot A/B com maior contador de
// save entre os que têm as 14 seções presentes e com checksum válido; se só
// um dos dois estiver íntegro, usa esse. Se nenhum estiver, retorna
// `valid=false` em vez de travar.
Gen3ParsedSave gen3_parse_save_streaming(Gen3SectionReader read_section, void *user_data);

// Versão leve pra quem só precisa da dex: valida os slots como acima, mas
// copia só o Small block (seção 0, GEN3_SMALL_BLOCK_SIZE bytes) pra
// `out_small`. Não monta o Gen3ParsedSave de ~19,8 KB, que no PokEsp estourou
// a pilha. Retorna false se nenhum slot estiver íntegro.
bool gen3_read_small_block(Gen3SectionReader read_section, void *user_data, uint8_t *out_small,
                           Gen3GameVersion *out_version);

// Conveniência sobre gen3_parse_save_streaming pra quando o save já está
// inteiro em RAM (usado pelos testes nativos, que constroem saves sintéticos
// em memória — no device real, prefira gen3_parse_save_streaming direto do
// SD). `save_data_size` precisa ser >= 128KB; dumps de emulador com bytes
// extras no final são tolerados, só os primeiros 128KB importam.
Gen3ParsedSave gen3_parse_save(const uint8_t *save_data, size_t save_data_size);
