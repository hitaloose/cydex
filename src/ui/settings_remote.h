#pragma once

struct SettingsPatch;

// PUT /settings (api 6): aplica o que o app do computador mandou, grava na
// NVS e atualiza o que está na tela (brilho na hora; a tela aberta remonta
// se mostra o que mudou). O pedido já vem validado (settings_patch_parse).
void settings_remote_apply(const SettingsPatch &patch);
