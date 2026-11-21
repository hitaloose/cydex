#pragma once

#include <cstddef>

// Nome de save vindo da API (/saves/<nome>, em URL). Decodifica %XX e '+' e
// confere: até SAVE_NAME_MAX bytes, termina em .sav ou .srm (sem diferenciar
// maiúsculas), ASCII imprimível sem / \ : * ? " < > |, não começa com '.'
// nem com espaço. Nomes no-intro ("Pokemon - Emerald Version (USA).sav")
// passam. false = nome inválido (a API responde 400).
constexpr size_t SAVE_NAME_MAX = 64;
bool save_name_decode(const char *in, char *out, size_t cap);
