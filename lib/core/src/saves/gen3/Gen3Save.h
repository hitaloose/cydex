#pragma once

#include "saves/SaveFormat.h"

// Leitura aos pedaços de um save Gen3 aberto (SaveHandle), sem montar os
// blocos inteiros na RAM: cada acesso vai direto no offset do arquivo.
bool gen3_open(SaveHandle &h);
bool gen3_summarize(const SaveHandle &h, SaveSummary &out);
uint8_t gen3_read_party(const SaveHandle &h, SaveMon out[6]);
bool gen3_read_box(const SaveHandle &h, uint8_t box, SaveMon out[SAVE_BOX_SLOTS], char *name,
                   size_t name_size);
uint8_t gen3_read_hof_team(const SaveHandle &h, uint8_t team, SaveMon out[6]);
