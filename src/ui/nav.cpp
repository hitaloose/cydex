#include "nav.h"

#include <lvgl.h>

#include "ui/catalog.h"
#include "ui/screens/screens.h"

static Screen s_current = Screen::Boot;
static Screen s_previous = Screen::Menu;

static lv_obj_t *build(Screen s) {
  switch (s) {
    case Screen::Boot: return boot_screen_create();
    case Screen::SdError: return sd_error_screen_create();
    case Screen::Menu: return menu_screen_create();
    case Screen::List: return list_screen_create();
    case Screen::Grid: return grid_screen_create();
    case Screen::GoTo: return goto_screen_create();
    case Screen::Search: return search_screen_create();
    case Screen::Types: return types_screen_create();
    case Screen::Progress: return progress_screen_create();
    case Screen::Settings: return settings_screen_create();
    case Screen::SettingsShowcase: return settings_showcase_screen_create();
    case Screen::SettingsSystem: return settings_system_screen_create();
    case Screen::Detail: return detail_screen_create();
    case Screen::Showcase: return showcase_screen_create();
  }
  return menu_screen_create();  // inalcançável: o switch cobre todas as telas
}

void nav_reload() { nav_go(s_current); }

void nav_go(Screen s) {
  if (s != s_current) s_previous = s_current;  // nav_reload não muda a origem
  s_current = s;
  // A tela anterior é apagada ANTES de montar a próxima. Dois motivos:
  //  - as telas guardam estado em estáticos (sprites, linhas); Ficha -> Ficha
  //    com a antiga viva fazia o DELETE dela liberar o sprite da nova e vazar
  //    o dela (18 KB por toque);
  //  - sem PSRAM, não dá pra ter os sprites das duas telas ao mesmo tempo.
  // Tela vazia intermediária: o LVGL não desenha nada entre as duas chamadas
  // (é tudo síncrono), então não pisca. auto_del=true dispara LV_EVENT_DELETE,
  // onde cada tela libera sprites e timers. Tempo 0 = sem animação.
  lv_obj_t *blank = lv_obj_create(nullptr);
  lv_scr_load_anim(blank, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
  lv_obj_t *next = build(s);
  lv_scr_load_anim(next, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

Screen nav_current() { return s_current; }
Screen nav_previous() { return s_previous; }
