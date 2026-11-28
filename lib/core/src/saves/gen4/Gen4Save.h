#pragma once

#include "saves/SaveFormat.h"

// HeartGold/SoulSilver: dumps de 512 KB (.sav cru do melonDS/DeSmuME). Cobre
// a dex nacional 1..493. D/P/Pt = outra entrada em Gen4Layout.
extern const SaveFormat GEN4_SAVE_FORMAT;

// CRC16-CCITT (polinômio 0x1021, começa em 0xFFFF), incremental: passar o
// valor anterior em `crc` pra continuar.
uint16_t gen4_crc16(const uint8_t *data, size_t len, uint16_t crc = 0xFFFF);
