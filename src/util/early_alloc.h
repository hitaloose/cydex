#pragma once

#include <cstddef>

// Buffers fixos do boot (buffer do display 15 KB + nomes das espécies 16 KB)
// saem de UM bloco de heap pedido logo no começo do setup.
//
// Por quê: com o Wi-Fi a .bss estática não cabe na região do linker, então
// eles saíram da .bss. Pedidos separados caíam nas regiões pequenas do heap
// (24 e 22 KB) e roubavam o espaço dos sprites; num bloco de 31 KB só a
// região grande cabe, e as pequenas ficam livres como antes.
// Nunca é liberado. Chamar early_alloc_init() antes de qualquer early_alloc().
void early_alloc_init();
void *early_alloc(size_t size);
