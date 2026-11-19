#pragma once

#include <lvgl.h>

#include "ui/fonts/fonts.h"

// Tokens do CLAUDE.md (seção "Tokens"). Mudou lá, muda aqui.
namespace theme {
constexpr uint32_t BG = 0x0F1317;
constexpr uint32_t SURFACE = 0x1A2027;
constexpr uint32_t SURFACE_2 = 0x232B34;
constexpr uint32_t LINE = 0x2C3540;
constexpr uint32_t ROW_DIVIDER = 0x1F262E;
constexpr uint32_t TEXT = 0xECEFF2;
constexpr uint32_t MUTED = 0x93A0AD;
constexpr uint32_t FAINT = 0x4A5562;
constexpr uint32_t FAINT_2 = 0x3A4552;
constexpr uint32_t ACCENT = 0xFF5A4E;
constexpr uint32_t ON_ACCENT = 0x0F1317;  // texto sobre accent e chips de tipo
constexpr uint32_t ACCENT_SOFT = 0x2A1A1A;  // fundo do selo "Capturado" da Ficha
constexpr uint32_t SURFACE_DIM = 0x161B21;  // moldura do sprite de espécie não vista
// Avisos do Claude Code (15, 15b): permissão usa o accent.
constexpr uint32_t NOTICE_IDLE = 0xF5D547;  // = Elétrico
constexpr uint32_t NOTICE_DONE = 0x6FCF6A;  // = Grama

// Medidas fixas (CLAUDE.md, "Regras de UI").
constexpr lv_coord_t HEADER_H = 32;
constexpr lv_coord_t HEADER_SEARCH_H = 36;
constexpr lv_coord_t TABS_H = 38;
constexpr lv_coord_t PAGER_H = 38;
constexpr lv_coord_t PAGER_BTN_W = 72;
constexpr lv_coord_t ROW_H = 34;
constexpr lv_coord_t MIN_TOUCH = 32;
}  // namespace theme

inline lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }
