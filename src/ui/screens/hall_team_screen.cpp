// 19a Hall da Fama: um time (canvas: HallTime.dc.html).
//   cabeçalho: voltar, "#5 · Ana", chip do jogo, ‹ › pro time vizinho na
//   ordem da lista do 19
//   faixa: troféu + "Vitória mais recente" / "1ª vitória com Xh de jogo" /
//   "Vitória nº n", e "n de N" à direita
//   3x2 cards com sprite 48, apelido, espécie e nível; toque abre a Ficha
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "data/SpeciesRepo.h"
#include "i18n/i18n.h"
#include "screens.h"
#include "ui/icons/icons.h"
#include "ui/saves_ui.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

struct Mem {
  SaveMon team[6];
  Sprite sprites[6];
  char sub[6][32];
  HofRef refs[16 * SAVE_HOF_MAX];
};
static_assert(sizeof(Mem) <= SAVES_SCREEN_MEM, "Time do Hall não cabe na área das telas");
Mem *s_m;
#define s_team (s_m->team)
#define s_sprites (s_m->sprites)
#define s_sub (s_m->sub)

void on_back(lv_event_t *) { nav_go(Screen::HallOfFame); }

void on_neighbor(lv_event_t *e) {
  saves_state().hof_index = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
  nav_go(Screen::HallTeam);
}

void nav_button(lv_obj_t *h, const lv_img_dsc_t *icon, int32_t target, uint16_t count) {
  bool ok = target >= 0 && target < count;
  lv_obj_t *b = ui_header_button(h, icon, on_neighbor, (void *)(uintptr_t)(ok ? target : 0));
  if (!ok) {
    lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_recolor(lv_obj_get_child(b, 0), c(theme::FAINT_2), 0);
  }
}

void on_mon(lv_event_t *e) {
  uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  open_species_from(s_team[i].species, Screen::HallTeam);
}

void on_delete(lv_event_t *) {
  if (s_m) {
    for (Sprite &s : s_sprites) sprite_free(&s);
  }
  saves_screen_mem_release();
  s_m = nullptr;
}

}  // namespace

lv_obj_t *hall_team_screen_create() {
  s_m = (Mem *)saves_screen_mem_take(sizeof(Mem));
  lv_obj_t *scr = ui_screen();
  lv_obj_add_event_cb(scr, on_delete, LV_EVENT_DELETE, nullptr);
  SavesState &st = saves_state();
  if (!s_m) {
    ui_header(scr, tr(STR_TITLE_HOF), on_back);
    return scr;
  }
  const HofRef *refs = s_m->refs;
  uint16_t count = hall_list_build(s_m->refs, sizeof(s_m->refs) / sizeof(s_m->refs[0]));
  SaveHead s;
  if (st.hof_index >= count || !save_store_head(refs[st.hof_index].save, &s)) {
    ui_header(scr, tr(STR_TITLE_HOF), on_back);
    return scr;
  }
  const HofRef r = refs[st.hof_index];
  uint8_t n = save_store_open(r.save) ? save_store_hof_team(r.team, s_team) : 0;

  char title[40];
  snprintf(title, sizeof(title), tr(STR_HOF_TITLE_FMT), r.team + 1, s.name);
  lv_obj_t *h = ui_header(scr, title, on_back);
  lv_obj_t *chip = ui_game_chip(h, s.game);
  lv_obj_set_style_translate_x(chip, 6, 0);
  ui_header_spacer(h);
  // A lista vai do mais novo pro mais velho: ‹ = mais novo (índice anterior).
  nav_button(h, &icon_prev_18, (int32_t)st.hof_index - 1, count);
  nav_button(h, &icon_forward_18, (int32_t)st.hof_index + 1, count);

  lv_obj_t *strip = ui_box(scr);
  lv_obj_set_pos(strip, 10, theme::HEADER_H + 6);
  lv_obj_set_size(strip, 300, 20);
  lv_obj_set_style_pad_column(strip, 6, 0);
  lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  bool latest = r.team + 1 == s.hof_count;
  ui_icon(strip, &icon_trophy_14, latest ? theme::NOTICE_IDLE : theme::MUTED);
  char buf[48];
  if (r.team == 0 && s.first_hof) {
    char t[20];
    format_packed_time(s.first_hof, t, sizeof(t));
    snprintf(buf, sizeof(buf), tr(STR_HOF_FIRST_LONG_FMT), t);
  } else if (latest) {
    snprintf(buf, sizeof(buf), "%s", tr(STR_HOF_LATEST));
  } else {
    snprintf(buf, sizeof(buf), tr(STR_HOF_NTH_FMT), r.team + 1);
  }
  lv_obj_t *text = ui_label(strip, &font_12_600, theme::TEXT, buf);
  lv_obj_set_flex_grow(text, 1);
  lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
  snprintf(buf, sizeof(buf), tr(STR_HOF_OF_FMT), r.team + 1, s.hof_count);
  ui_label(strip, &font_12_500, theme::MUTED, buf);

  constexpr lv_coord_t kPad = 6, kGap = 4, kTop = theme::HEADER_H + 6 + 20 + 4;
  constexpr lv_coord_t kW = (320 - 2 * kPad - 2 * kGap) / 3, kH = (240 - kTop - kPad - kGap) / 2;
  for (uint8_t i = 0; i < 6; i++) {
    lv_coord_t x = kPad + (i % 3) * (kW + kGap), y = kTop + (i / 3) * (kH + kGap);
    if (i >= n) {
      ui_mon_empty(scr, x, y, kW, kH, false);
      continue;
    }
    const SaveMon &m = s_team[i];
    const char *species = species_repo_name(m.species);
    // Sem apelido, o nome já é o da espécie (em maiúsculas no Gen3): embaixo vai o número.
    if (strcasecmp(m.nick, species) == 0) snprintf(s_sub[i], sizeof(s_sub[i]), "#%03u", m.species);
    else snprintf(s_sub[i], sizeof(s_sub[i]), "%s", species);
    MonCardSpec spec{m.species, m.nick, m.level, m.shiny, false, 0, 0, s_sub[i]};
    ui_mon_card(scr, x, y, kW, kH, spec, &s_sprites[i], on_mon, (void *)(uintptr_t)i);
  }
  return scr;
}
