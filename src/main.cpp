#include <Arduino.h>
#include <lvgl.h>

#include "board_pins.h"
#include "hal/backlight.h"
#include "hal/display.h"
#include "data/Settings.h"
#include "hal/touch.h"
#include "ui/nav.h"
#include "ui/power.h"
#include "util/log.h"

static const char *TAG = "main";

// A loopTask do arduino-esp32 tem 8 KB de pilha. No PokEsp a cadeia
// LVGL -> controller -> parser -> SD medida com -fstack-usage chegou a ~32,6 KB.
// 64 KB dá folga. Se entrar struct grande nessa cadeia, medir de novo com
// -fstack-usage em vez de estimar.
size_t getArduinoLoopTaskStackSize(void) { return 65536; }

void setup() {
  Serial.begin(115200);
  delay(100);

  // RGB onboard é comum-anodo: HIGH apaga (senão acende sozinho no boot).
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  digitalWrite(PIN_LED_RED, HIGH);
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_BLUE, HIGH);

  LOG_I(TAG, "CYDex — chip=%s %dMHz heap=%u maior_bloco=%u", ESP.getChipModel(),
        ESP.getCpuFreqMHz(), ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  display_init();
  backlight_init();
  lv_init();
  display_lvgl_init();
  touch_init();
  touch_lvgl_init();

  settings_init();
  power_init();  // brilho dos Ajustes, som ao tocar, apagar tela

  // O Boot monta o SD, lê espécies e saves e segue pro Menu (ou pro erro).
  nav_go(Screen::Boot);
}

void loop() {
  lv_timer_handler();
  power_tick();
  delay(5);
}
