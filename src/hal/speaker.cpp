#include "speaker.h"

#include <Arduino.h>

#include "board_pins.h"

// Canal 2 = timer 1 do ledc. O backlight está no canal 0 (timer 0); canais
// 0 e 1 dividem o timer, e mudar a frequência ali mexeria no PWM da tela.
static const int kChannel = 2;
static const uint32_t kToneHz = 2400;
static const uint32_t kClickMs = 12;
static uint32_t s_until = 0;
static bool s_on = false;

void speaker_init() {
  ledcSetup(kChannel, kToneHz, 8);
  ledcAttachPin(PIN_SPEAKER, kChannel);
  ledcWrite(kChannel, 0);
}

void speaker_click() {
  ledcWriteTone(kChannel, kToneHz);
  ledcWrite(kChannel, 32);  // duty baixo: clique discreto
  s_until = millis() + kClickMs;
  s_on = true;
}

void speaker_update() {
  if (s_on && (int32_t)(millis() - s_until) >= 0) {
    ledcWrite(kChannel, 0);
    s_on = false;
  }
}
