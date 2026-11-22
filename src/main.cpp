#include <Arduino.h>
#include <lvgl.h>

#include "board_pins.h"
#include "hal/backlight.h"
#include "hal/display.h"
#include "hal/sd.h"
#include "data/Settings.h"
#include "data/SpeciesRepo.h"
#include "data/SpriteLoader.h"
#include "hal/touch.h"
#include "net/wifi_mgr.h"
#include "ui/nav.h"
#include "ui/notice_center.h"
#include "ui/power.h"
#include "util/early_alloc.h"
#include "util/log.h"

static const char *TAG = "main";

// A loopTask do arduino-esp32 tem 8 KB de pilha. A cadeia mais funda do
// CYDex, medida com -fstack-usage, é o parse do save Gen3 (3 quadros de ~4,2 KB,
// ~12,6 KB) + LVGL. 32 KB dá 2,5x de folga. Era 64 KB (número do PokEsp), mas
// esses 64 KB saem do heap e o Wi-Fi precisa deles. Se entrar struct grande
// nessa cadeia, medir de novo com -fstack-usage em vez de estimar.
size_t getArduinoLoopTaskStackSize(void) { return 32768; }

// Comandos de linha pela serial. Hoje só "shot" (print da tela, ver
// tools/cydex_screenshot.py).
static void serial_cmd_tick() {
  static char line[24];
  static size_t len = 0;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (len < sizeof(line) - 1) line[len++] = c;
      continue;
    }
    line[len] = '\0';
    len = 0;
    // "shot" = tela toda; "shot <y0> <y1>" = só essas linhas (o script pede de
    // novo as faixas que chegaram com byte perdido).
    unsigned y0 = 0, y1 = 0;
    if (strcmp(line, "shot") == 0) display_capture_to_serial(0, BOARD_DISPLAY_HEIGHT - 1);
    else if (sscanf(line, "shot %u %u", &y0, &y1) == 2) display_capture_to_serial(y0, y1);
  }
}

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

  // Buffer do display + nomes num bloco só, antes de tudo (util/early_alloc).
  early_alloc_init();
  display_init();
  backlight_init();
  lv_init();
  display_lvgl_init();
  touch_init();
  touch_lvgl_init();

  settings_init();
  power_init();  // brilho dos Ajustes, som ao tocar, apagar tela
  species_repo_reserve();
  sprite_reserve();  // 46 KB fixos dos sprites, antes de o Wi-Fi picar o heap
  LOG_I(TAG, "reservas: heap=%u maior_bloco=%u", ESP.getFreeHeap(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  // SD antes do Wi-Fi: o FATFS pede ~17 KB contíguos, que o Wi-Fi pica. O Boot
  // encontra montado (sd_init não remonta); sem cartão, o Boot tenta de novo.
  sd_init();
  wifi_mgr_init();  // conecta sozinho se houver rede salva
  LOG_I(TAG, "wifi iniciado: heap=%u maior_bloco=%u", ESP.getFreeHeap(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  // O Boot monta o SD, lê espécies e saves e segue pro Menu (ou pro erro).
  nav_go(Screen::Boot);
}

void loop() {
  lv_timer_handler();
#if LOG_LEVEL >= 2
  // Memória a cada 10 s (-D LOG_LEVEL=2): pilha livre mínima da loopTask, pico
  // do pool do LVGL e heap. Usar depois de mexer em buffer grande ou no Wi-Fi.
  static uint32_t s_mem_at = 0;
  if (millis() - s_mem_at > 10000) {
    s_mem_at = millis();
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    LOG_D("mem", "pilha_livre_min=%u lv_max_usado=%u lv_frag=%u%% heap=%u maior_bloco=%u min_heap=%u",
          (unsigned)uxTaskGetStackHighWaterMark(nullptr), (unsigned)m.max_used, m.frag_pct,
          ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
          ESP.getMinFreeHeap());
  }
#endif
  wifi_tick();    // Wi-Fi + servidor de avisos (cydex.local/notices)
  notice_tick();  // avisos: faixa ou balão na Vitrine
  power_tick();
  serial_cmd_tick();
  delay(5);
}
