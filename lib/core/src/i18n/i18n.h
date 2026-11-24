#pragma once

/* Textos da interface.
 *
 * Adicionar um TEXTO:
 *   1. Criar a chave STR_* aqui (no grupo da tela).
 *   2. Escrever o texto em CADA arquivo de lang/. O teste nativo
 *      (test/test_i18n) falha se faltar algum.
 *
 * Adicionar um IDIOMA:
 *   1. Copiar lang/pt_br.c para lang/<código>.c e traduzir.
 *   2. Acrescentar LANG_<X> em LangId e uma linha em kLangs (i18n.c).
 *   Limite: as fontes cobrem Latin-1 (pt, en, es, fr, de, it...). Idioma fora
 *   disso (ja, ko...) exige outras fontes em tools/gen_fonts.mjs.
 *
 * Os arquivos de idioma são C, não C++, por causa do inicializador designado
 * ([STR_X] = "..."): a ordem não importa e uma chave com nome errado não
 * compila. Texto em UTF-8. Formatos de printf (%u) têm que ser os mesmos em
 * todos os idiomas.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  /* Boot */
  STR_BOOT_READING_SD,
  STR_BOOT_READING_SAVES,
  STR_BOOT_RECORDS_FMT, /* %u / %u */
  STR_BOOT_SAVES_FMT,   /* %u / %u */

  /* Sem cartão SD */
  STR_SDERR_NO_CARD_TITLE,
  STR_SDERR_NO_CARD_BODY,
  STR_SDERR_NO_DATA_TITLE,
  STR_SDERR_NO_DATA_BODY,
  STR_SDERR_RETRY,

  /* Menu */
  STR_MENU_CAUGHT,
  STR_MENU_SPECIES_FMT, /* %u */

  /* Títulos das telas (também usados nos tiles do Menu) */
  STR_TITLE_LIST,
  STR_TITLE_GRID,
  STR_TITLE_GOTO,
  STR_TITLE_SEARCH,
  STR_TITLE_TYPES,
  STR_TITLE_PROGRESS,
  STR_TITLE_SETTINGS,
  STR_TITLE_SETTINGS_SHOWCASE,
  STR_TITLE_SHOWCASE,

  /* Lista / Grade */
  STR_CATALOG_NATIONAL, /* título do cabeçalho */
  STR_FILTER_ALL,
  STR_FILTER_CAUGHT,
  STR_FILTER_MISSING,
  STR_CATALOG_EMPTY,    /* filtro sem resultado */

  /* Rodapé de paginação */
  STR_PAGER_PAGE_FMT,   /* %u de %u */
  STR_PAGER_EMPTY,

  /* Ir para */
  STR_GOTO_GENERATION,
  STR_GOTO_NUMBER,
  STR_GOTO_GO,

  /* Ficha */
  STR_DETAIL_TAB_INFO,
  STR_DETAIL_TAB_STATS,
  STR_DETAIL_TAB_EVOLUTION,
  STR_DETAIL_HEIGHT,
  STR_DETAIL_WEIGHT,
  STR_DETAIL_GENERATION,
  STR_DETAIL_HEIGHT_FMT, /* %s = "0,7" */
  STR_DETAIL_WEIGHT_FMT, /* %s = "6,9" */
  STR_STATE_CAUGHT,
  STR_STATE_SEEN,
  STR_STATE_UNSEEN,
  STR_STAT_TOTAL,
  STR_EVO_LEVEL_FMT,     /* %u = nível */
  STR_EVO_NONE,          /* espécie sem evolução */

  /* Busca */
  STR_SEARCH_PLACEHOLDER,
  STR_SEARCH_RESULT_ONE,
  STR_SEARCH_RESULTS_FMT, /* %u >= 2 ou 0 */
  STR_SEARCH_SPACE,
  STR_SEARCH_SEE,
  STR_SEARCH_MODE_NUM,
  STR_SEARCH_MODE_ABC,
  STR_SEARCH_CHIP_FMT, /* %s = texto buscado */

  /* Tipos */
  STR_TYPES_TITLE,
  STR_TYPES_UP_TO,
  STR_TYPES_CLEAR,
  STR_TYPES_APPLY,

  /* Progresso */
  STR_PROGRESS_CAUGHT_LABEL, /* embaixo do % no anel */
  STR_PROGRESS_CAUGHT_SHORT,
  STR_PROGRESS_SEEN,

  /* Ajustes */
  STR_SET_BRIGHTNESS,
  STR_SET_SOUND,
  STR_SET_OPEN_IN,
  STR_SET_SCREEN_OFF,
  STR_SET_MINUTES_FMT,
  STR_SET_SECONDS_FMT,
  STR_SET_NEVER,
  STR_SET_SYSTEM,
  STR_SET_DISPLAY,        /* grupo Tela e som (05a) */
  STR_SET_NAVIGATION,     /* grupo Navegação (05f) */
  STR_SET_SC_BRIGHTNESS,  /* brilho da vitrine, em Tela e som */
  STR_SET_BRIGHT_NIGHT,   /* brilho depois do pôr do sol */
  STR_SET_SC_BRIGHT_NIGHT,
  STR_SET_SUNSET,         /* linha "Pôr do sol" (só leitura) */
  STR_SET_SUNSET_NO_LOC,  /* sem localidade: o app do computador manda */
  STR_SET_SUNSET_NO_TIME, /* sem hora: o NTP vem pelo Wi-Fi */
  STR_SET_BOOT_MENU,      /* "Ao ligar": Menu | Vitrine */

  /* Ajustes › Modo vitrine */
  STR_SC_INTERVAL,
  STR_SC_ORDER,
  STR_SC_ORDER_SEQ,
  STR_SC_ORDER_RAND,
  STR_SC_ORDER_SEQ_SHORT,
  STR_SC_ORDER_RAND_SHORT,
  STR_SC_SHOW,
  STR_SC_SHOW_SEEN,
  STR_SC_SHOW_ALL,
  STR_SC_START,
  STR_SC_ON_BOOT,
  STR_SC_SUMMARY_H_FMT,
  STR_SC_SUMMARY_MIN_FMT,

  /* Ajustes › Sistema */
  STR_SYS_LANGUAGE,
  STR_SYS_SD,
  STR_SYS_SD_INFO_FMT,
  STR_SYS_RELOAD,

  /* Vitrine */
  STR_SC_FOOTER_FMT,  /* %u = nº, %s = geração em romano */
  STR_SC_EMPTY_CAUGHT,
  STR_SC_EMPTY_SEEN,
  STR_SC_EMPTY_HINT,
  // Folha de resposta da Vitrine (15c).
  STR_SC_ASK_DENY,
  STR_SC_ASK_ALLOW,
  STR_SC_ASK_HINT,
  STR_SC_ASK_NO_DETAIL,

  /* Wi-Fi (05c, 05d, 05d', 05e) e teclado da senha */
  STR_SET_WIFI,  /* linha do 05c e título do 05d */
  STR_WIFI_OFF,
  STR_WIFI_DISCONNECTED,
  STR_WIFI_SCANNING,
  STR_WIFI_SCANNING_LONG,
  STR_WIFI_CONNECTING,
  STR_WIFI_CONNECTED,
  STR_WIFI_NETWORKS_ONE,
  STR_WIFI_NETWORKS_FMT,  /* %u >= 2 ou 0 */
  STR_WIFI_NONE,
  STR_WIFI_OPEN,  /* rede sem senha */
  STR_WIFI_FORGET,
  STR_WIFI_OFF_TITLE,
  STR_WIFI_OFF_BODY,
  STR_WIFI_PASSWORD,  /* placeholder */
  STR_WIFI_CONNECT,
  STR_WIFI_DONE,
  STR_WIFI_WRONG_PASSWORD,
  STR_WIFI_NOT_FOUND,
  STR_WIFI_FAILED,
  STR_WIFI_INFO_TITLE,
  STR_WIFI_INFO_ADDRESS,
  STR_WIFI_INFO_IP,
  STR_WIFI_INFO_KEY,
  STR_WIFI_INFO_PC,
  STR_WIFI_INFO_HELP_FMT,  /* %s = chave */
  STR_WIFI_INFO_EXAMPLE,
  STR_KB_MODE_LOWER,  /* teclado da senha */
  STR_KB_MODE_SYMBOLS,

  /* Genéricos */
  STR_UNKNOWN_NAME,  /* espécie desconhecida */
  STR_DECIMAL_SEP,   /* "," ou "." em altura/peso */

  /* Tipos: MESMA ORDEM de SpeciesType (dex/Types.h). Testado. */
  STR_TYPE_NORMAL,
  STR_TYPE_FIRE,
  STR_TYPE_WATER,
  STR_TYPE_GRASS,
  STR_TYPE_ELECTRIC,
  STR_TYPE_ICE,
  STR_TYPE_FIGHTING,
  STR_TYPE_POISON,
  STR_TYPE_GROUND,
  STR_TYPE_FLYING,
  STR_TYPE_PSYCHIC,
  STR_TYPE_BUG,
  STR_TYPE_ROCK,
  STR_TYPE_GHOST,
  STR_TYPE_DRAGON,
  STR_TYPE_DARK,
  STR_TYPE_STEEL,
  STR_TYPE_FAIRY,

  /* Tipos abreviados (3 letras, chips da Vitrine): mesma ordem. Testado. */
  STR_TYPE_ABBR_NORMAL,
  STR_TYPE_ABBR_FIRE,
  STR_TYPE_ABBR_WATER,
  STR_TYPE_ABBR_GRASS,
  STR_TYPE_ABBR_ELECTRIC,
  STR_TYPE_ABBR_ICE,
  STR_TYPE_ABBR_FIGHTING,
  STR_TYPE_ABBR_POISON,
  STR_TYPE_ABBR_GROUND,
  STR_TYPE_ABBR_FLYING,
  STR_TYPE_ABBR_PSYCHIC,
  STR_TYPE_ABBR_BUG,
  STR_TYPE_ABBR_ROCK,
  STR_TYPE_ABBR_GHOST,
  STR_TYPE_ABBR_DRAGON,
  STR_TYPE_ABBR_DARK,
  STR_TYPE_ABBR_STEEL,
  STR_TYPE_ABBR_FAIRY,

  /* Status base, rótulo curto: mesma ordem do species.bin (PS..Vel) */
  STR_STAT_HP,
  STR_STAT_ATK,
  STR_STAT_DEF,
  STR_STAT_SPA,
  STR_STAT_SPD,
  STR_STAT_SPE,

  /* Saves e Hall da Fama (16–19a), Ficha › Onde está, Vitrine › Meu time */
  STR_TITLE_SAVES,
  STR_TITLE_HOF,
  STR_SAVES_COUNT_FMT,  /* %u = saves */
  STR_SAVES_COUNT_ONE,  /* "1 save" */
  STR_SAVES_SUB_FMT,  /* %s = tempo, %u/%u = dex */
  STR_SAVES_UNKNOWN,
  STR_SAVES_EMPTY_TITLE,
  STR_SAVES_EMPTY_BODY,
  STR_PLAYTIME_FMT,  /* %u = horas, %02u = minutos */
  STR_SAVE_TAB_SUMMARY,
  STR_SAVE_TAB_PARTY,
  STR_SAVE_TAB_BOXES,
  STR_SAVE_ID_FMT,  /* %05u = TID, %s = arquivo */
  STR_SAVE_TIME,
  STR_SAVE_BADGES,
  STR_SAVE_DEX,
  STR_SAVE_DEX_FMT,  /* %u = vistos, %u = total */
  STR_SAVE_MONEY,
  STR_SAVE_MONEY_FMT,  /* %s = valor com separador */
  STR_SAVE_HOF_FMT,  /* %u = times */
  STR_SAVE_HOF_ONE,  /* "1 time" */
  STR_SAVE_SHINY,
  STR_SAVE_EMPTY_SLOT,
  STR_SAVE_LEVEL,
  STR_BOX_TOTAL_FMT,  /* %u = criaturas nas caixas */
  STR_BOX_COUNT_FMT,  /* %u/30, %u de %u caixas */
  STR_BOX_EMPTY,
  STR_BOX_OPEN,
  STR_BOX_SUB_FMT,  /* %s = nível, %03u = nº */
  STR_MEMBER_TAB_MOVES,
  STR_MEMBER_PARTY_FMT,  /* %s = treinador, %u/%u = vaga */
  STR_MEMBER_BOX_FMT,  /* %s = treinador, %s = caixa */
  STR_MEMBER_HOF_FMT,  /* %s = treinador, %u = time */
  STR_MEMBER_NATURE,
  STR_MEMBER_ITEM,
  STR_MEMBER_OT,
  STR_MEMBER_OT_FMT,  /* %s = nome, %05u = TID */
  STR_MEMBER_NONE,
  STR_MEMBER_SHINY,
  STR_MEMBER_EGG,
  STR_MEMBER_PP,
  STR_MEMBER_SPECIES_FMT,  /* %s = espécie, %03u = nº */
  STR_HOF_ALL_FMT,  /* %u = times */
  STR_HOF_ALL_SAVES,
  STR_HOF_SAVE_FMT,  /* %s = treinador, %s = jogo */
  STR_HOF_FIRST_FMT,  /* %s = tempo de jogo */
  STR_HOF_MAX_LEVEL_FMT,  /* %u = nível */
  STR_HOF_FIRST_LONG_FMT,  /* %s = tempo de jogo */
  STR_HOF_LATEST,
  STR_HOF_NTH_FMT,  /* %u = nº do time */
  STR_HOF_OF_FMT,  /* %u de %u */
  STR_HOF_TITLE_FMT,  /* %u = nº, %s = treinador */
  STR_HOF_EMPTY,
  STR_DETAIL_WHERE,
  STR_WHERE_PARTY_FMT,  /* %s = treinador */
  STR_WHERE_BOX_FMT,  /* %u = caixa, %s = treinador */
  STR_SC_SHOW_TEAM,
  STR_SC_TEAM_FOOTER_FMT,  /* %03u = nº, %s = espécie */
  /* Naturezas, na ordem do jogo (0..24): STR_NATURE_HARDY + n */
  STR_NATURE_HARDY,
  STR_NATURE_LONELY,
  STR_NATURE_BRAVE,
  STR_NATURE_ADAMANT,
  STR_NATURE_NAUGHTY,
  STR_NATURE_BOLD,
  STR_NATURE_DOCILE,
  STR_NATURE_RELAXED,
  STR_NATURE_IMPISH,
  STR_NATURE_LAX,
  STR_NATURE_TIMID,
  STR_NATURE_HASTY,
  STR_NATURE_SERIOUS,
  STR_NATURE_JOLLY,
  STR_NATURE_NAIVE,
  STR_NATURE_MODEST,
  STR_NATURE_MILD,
  STR_NATURE_QUIET,
  STR_NATURE_BASHFUL,
  STR_NATURE_RASH,
  STR_NATURE_CALM,
  STR_NATURE_GENTLE,
  STR_NATURE_SASSY,
  STR_NATURE_CAREFUL,
  STR_NATURE_QUIRKY,

  STR_COUNT
} StrId;

typedef enum {
  LANG_PT_BR,
  LANG_EN,
  LANG_COUNT
} LangId;

/* Texto no idioma atual. Chave sem tradução cai no pt-BR. Nunca NULL. */
const char *tr(StrId id);

void i18n_set_lang(LangId lang);
LangId i18n_lang(void);

/* "pt-BR", "en"... (salvo na NVS) */
const char *i18n_lang_code(LangId lang);
/* Nome do idioma escrito nele mesmo ("Português", "English"), pros Ajustes. */
const char *i18n_lang_name(LangId lang);
/* Código desconhecido -> LANG_PT_BR. */
LangId i18n_lang_from_code(const char *code);

/* Texto cru da tabela de um idioma (NULL se faltar). Para os testes. */
const char *i18n_raw(LangId lang, StrId id);

#ifdef __cplusplus
}
#endif
