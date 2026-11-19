#pragma once

#include "Gen3SaveParser.h"
#include "models/Trainer.h"

// Extrai nome, ID, badges e tempo de jogo de um save já parseado
// (gen3_parse_save). Comportamento indefinido se `save.valid == false` —
// sempre checar antes de chamar.
Trainer gen3_extract_trainer_info(const Gen3ParsedSave &save);
