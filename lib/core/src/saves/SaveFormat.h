#pragma once

#include <cstddef>
#include <cstdint>

#include "dex/DexStatus.h"

// Ponte entre o arquivo de save (no SD ou em memória nos testes) e os
// parsers. Lê `len` bytes a partir de `offset`.
struct SaveReader {
  size_t size;
  bool (*read)(size_t offset, uint8_t *out, size_t len, void *user);
  void *user;
};

// Uma geração de save. Para suportar uma geração nova, criar outro
// SaveFormat e incluir em kFormats (SaveFormat.cpp). O resto do firmware
// não muda.
struct SaveFormat {
  const char *name;
  // Barato: normalmente só olha o tamanho.
  bool (*detect)(const SaveReader &r);
  // Faz OR dos bits visto/capturado deste save em `out` (dex nacional).
  bool (*merge_dex)(const SaveReader &r, DexStatus &out);
};

// Tenta cada formato conhecido. Retorna o nome do formato usado, ou nullptr
// se nenhum reconhecer o arquivo ou o save for inválido.
const char *save_merge_dex(const SaveReader &r, DexStatus &out);
