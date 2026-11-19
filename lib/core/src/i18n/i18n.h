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

  /* Ajustes › Modo vitrine */
  STR_SC_INTERVAL,
  STR_SC_ORDER,
  STR_SC_ORDER_SEQ,
  STR_SC_ORDER_RAND,
  STR_SC_ORDER_SEQ_SHORT,
  STR_SC_ORDER_RAND_SHORT,
  STR_SC_SHOW,
  STR_SC_SHOW_SEEN,
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
  STR_SC_TOTAL_FMT,
  STR_SC_EMPTY_CAUGHT,
  STR_SC_EMPTY_SEEN,
  STR_SC_EMPTY_HINT,

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

  /* Avisos do Claude Code (15, 15b) */
  STR_NOTICE_PERMISSION,  /* faixa (15) */
  STR_NOTICE_IDLE,
  STR_NOTICE_DONE,
  STR_NOTICE_PERMISSION_SHORT,  /* balão da Vitrine (15b) */
  STR_NOTICE_IDLE_SHORT,
  STR_NOTICE_DONE_SHORT,

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

  /* Status base, rótulo curto: mesma ordem do species.bin (PS..Vel) */
  STR_STAT_HP,
  STR_STAT_ATK,
  STR_STAT_DEF,
  STR_STAT_SPA,
  STR_STAT_SPD,
  STR_STAT_SPE,

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
