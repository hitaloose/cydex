#pragma once

#include <cstddef>
#include <cstdint>

// Texto do Gen4: u16 LE por caractere, fim em 0xFFFF. Só o pedaço latino da
// tabela do PKHeX (StringConverter4Util.TableINT); kana, hangul e símbolos
// viram "?" (o text_fit da UI cuida do que a fonte não tem). `chars` =
// máximo de caracteres a ler; `out` sempre termina em '\0'.
void gen4_text_decode(const uint8_t *data, size_t chars, char *out, size_t out_size);

// Caminho inverso, só ASCII (os testes montam saves com isto). Escreve até
// `chars` u16 contando o 0xFFFF; o resto fica 0.
void gen4_text_encode(const char *text, uint8_t *out, size_t chars);
