// 01 Boot (canvas: Main.dc.html). Logo + barra "Lendo cartão SD… N / 1025".
// Também é a tela do "Tentar de novo" e do "Cartão SD › recarregar".
#include <Arduino.h>

#include "data/Settings.h"
#include "data/SpeciesRepo.h"
#include "data/StatusStore.h"
#include "hal/sd.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/catalog.h"
#include "ui/nav.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "util/log.h"

static const char *TAG = "boot";
static const char *APP_VERSION = "v0.1";

static lv_obj_t *s_bar;
static lv_obj_t *s_title;
static lv_obj_t *s_detail;

// Fases: nomes das espécies (0–90% da barra) e saves (90–100%).
static void on_species(uint16_t done, uint16_t total, void *) {
  // Cada redesenho custa ~10 ms: atualiza a cada 128 registros (~9 vezes).
  if (done % 128 != 0 && done != total) return;
  ui_progress_set(s_bar, (int32_t)done * 90, (int32_t)total * 100);
  lv_label_set_text_fmt(s_detail, tr(STR_BOOT_RECORDS_FMT), done, total);
  lv_refr_now(nullptr);  // carga bloqueante: redesenha só a área que mudou
}

static void on_saves(uint8_t done, uint8_t total, void *) {
  ui_progress_set(s_bar, 90 + (total ? 10 * done / total : 10), 100);
  lv_label_set_text_fmt(s_detail, tr(STR_BOOT_SAVES_FMT), done, total);
  lv_refr_now(nullptr);
}

static bool s_force_rescan = false;
void boot_request_rescan() { s_force_rescan = true; }

static void load(lv_timer_t *t) {
  lv_timer_del(t);
  uint32_t t0 = millis();

  if (!sd_init()) {
    sd_error_set_reason(SdErrorReason::NoCard);
    nav_go(Screen::SdError);
    return;
  }
  if (!species_repo_init(on_species, nullptr)) {
    sd_error_set_reason(SdErrorReason::NoData);
    nav_go(Screen::SdError);
    return;
  }
  uint32_t t1 = millis();

  lv_label_set_text(s_title, tr(STR_BOOT_READING_SAVES));
  // "Recarregar" (Ajustes › Sistema) ignora o status.cache uma vez.
  bool rescan = s_force_rescan;
  StatusLoadReport rep = status_store_load(rescan, on_saves, nullptr);
  s_force_rescan = false;
  catalog_invalidate();  // o filtro Capturados/Faltando depende do status
  LOG_I(TAG, "espécies %ums, saves %ums (%u/%u, cache=%d) heap=%u maior_bloco=%u", t1 - t0,
        millis() - t1, rep.saves_parsed, rep.saves_found, rep.from_cache, ESP.getFreeHeap(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  // Ajustes › Modo vitrine › Iniciar ao ligar. "Recarregar" volta pro Menu.
  nav_go(settings().showcase_on_boot && !rescan ? Screen::Showcase : Screen::Menu);
}

lv_obj_t *boot_screen_create() {
  lv_obj_t *scr = ui_screen();

  lv_obj_t *col = ui_box(scr);
  lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 16, 0);

  lv_obj_t *brand = ui_box(col);
  lv_obj_set_size(brand, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(brand, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(brand, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(brand, 10, 0);
  ui_logo(brand, 28);
  ui_label(brand, &font_28_700, theme::TEXT, "CYDex");

  lv_obj_t *prog = ui_box(col);
  lv_obj_set_size(prog, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(prog, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(prog, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(prog, 6, 0);
  s_bar = ui_progress(prog, 180, 6);
  s_title = ui_label(prog, &font_12_600, theme::TEXT, tr(STR_BOOT_READING_SD));
  s_detail = ui_label(prog, &font_12_500, theme::MUTED, "");

  lv_obj_t *ver = ui_label(scr, &font_12_500, theme::MUTED, APP_VERSION);
  lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -8);

  // Deixa a tela aparecer antes de começar a carga bloqueante.
  lv_timer_create(load, 30, nullptr);
  return scr;
}
