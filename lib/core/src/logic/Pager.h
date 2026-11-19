#pragma once

#include <cstdint>

// Paginação sem scroll (Lista, Grade). Páginas a partir de 0.
struct Pager {
  uint16_t total = 0;     // itens
  uint8_t per_page = 1;
  uint16_t page = 0;

  uint16_t page_count() const;
  uint16_t first_index() const;  // índice (0-based) do 1o item da página
  uint16_t items_on_page() const;
  bool has_prev() const { return page > 0; }
  bool has_next() const { return page + 1 < page_count(); }
  // Retornam true se a página mudou (=> redesenhar).
  bool prev();
  bool next();
  bool go_to_page(uint16_t p);
  // Vai pra página que contém o item de índice `index`.
  bool go_to_index(uint16_t index);
  void reset(uint16_t total_items, uint8_t items_per_page);
};

// Repetição ao segurar ◀ ▶ (igual ao canvas): primeiro passo imediato,
// espera 450 ms pra começar a repetir (evita repetição sem querer), depois
// 250 ms entre passos, caindo pra 90 ms depois de 8 passos.
constexpr uint32_t PAGER_REPEAT_FIRST_MS = 450;
constexpr uint32_t PAGER_REPEAT_SLOW_MS = 250;
constexpr uint32_t PAGER_REPEAT_FAST_MS = 90;
constexpr uint16_t PAGER_REPEAT_ACCEL_AFTER = 8;

struct HoldRepeat {
  uint16_t steps = 0;
  uint32_t next_at = 0;
  bool active = false;

  // Dedo encostou: já conta um passo.
  void press(uint32_t now_ms);
  // Chamar enquanto segura. true = dar mais um passo agora.
  bool poll(uint32_t now_ms);
  void release() { active = false; }
};
