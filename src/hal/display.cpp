#include "display.h"

#include "board_pins.h"
#include "util/log.h"

static const char *TAG = "display";

TFT_eSPI tft;

// Sem PSRAM: buffer parcial (320 x 24 linhas = 15 KB) em DRAM interna.
// Nunca um frame buffer inteiro (150 KB).
#define DISP_BUF_LINES 24

static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_buf[BOARD_DISPLAY_WIDTH * DISP_BUF_LINES];
static lv_disp_drv_t disp_drv;

void display_init() {
  tft.init();
  tft.setRotation(1);  // paisagem
  tft.fillScreen(TFT_BLACK);
  LOG_I(TAG, "TFT ok — %dx%d, rotation=1", tft.width(), tft.height());
}

void display_self_test() {
  const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_WHITE};
  const int n = sizeof(colors) / sizeof(colors[0]);
  const int w = tft.width() / n;
  for (int i = 0; i < n; i++) {
    tft.fillRect(i * w, 0, w, tft.height(), colors[i]);
  }
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("CYDex", tft.width() / 2, tft.height() / 2);
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

void display_lvgl_init() {
  lv_disp_draw_buf_init(&draw_buf, disp_buf, nullptr, BOARD_DISPLAY_WIDTH * DISP_BUF_LINES);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = BOARD_DISPLAY_WIDTH;
  disp_drv.ver_res = BOARD_DISPLAY_HEIGHT;
  disp_drv.flush_cb = flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  LOG_I(TAG, "LVGL: buffer parcial de %d linhas (%u bytes)", DISP_BUF_LINES,
        (unsigned)sizeof(disp_buf));
}
