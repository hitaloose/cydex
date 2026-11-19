#pragma once

#include <cstdint>

#include "dex/DexStatus.h"

// Status visto/capturado de todas as espécies, montado a partir dos saves
// em /saves. Lido UMA vez no boot (ou ao recarregar o cartão) e mantido em
// RAM: Lista, Grade, Progresso e Vitrine só consultam status_store_get().

struct StatusLoadReport {
  uint8_t saves_found;
  uint8_t saves_parsed;  // reconhecidos e válidos
  bool from_cache;
};

// `done`/`total` = saves processados. Usado pela barra do Boot.
typedef void (*StatusProgressCb)(uint8_t done, uint8_t total, void *user);

// force=true ignora o cache (Ajustes › Cartão SD › recarregar).
StatusLoadReport status_store_load(bool force, StatusProgressCb cb, void *user);
const DexStatus &status_store_get();
// Resultado da última leitura (Ajustes › Sistema: "4 saves").
const StatusLoadReport &status_store_last_report();
