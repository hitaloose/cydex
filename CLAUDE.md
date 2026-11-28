# CYDex

Pokédex de mesa rodando num **CYD (Cheap Yellow Display, ESP32-2432S028)** de 2,8". Mostra o catálogo de espécies (capturadas, vistas ou desconhecidas) lido do cartão SD, tem um **modo vitrine** sempre ligado e recebe pelo Wi-Fi **avisos** de qualquer coisa na rede (API HTTP `/notices`; o Claude Code é um dos clientes).

Nome: **CYDex** (CYD + dex, lê-se "sai-dex"). Não usar "Poké"/"Pokémon" no nome do projeto, do repositório ou de binários; "Dex" sozinho é genérico.

## Layout (fonte da verdade da UI)

- Canvas com todas as telas: https://claude.ai/artifact/3cxMPNMPxtGhqaFMbF4uFy. Duas páginas: **CYD · 320×240** (o firmware, tabela abaixo) e **App do computador** (o `desktop/`, tabela em "App do computador").
- Cada tela do CYD é um artboard de **320×240** (tamanho real do display); as do app, de 760×560 (a janela). No modo Play elas navegam entre si.
- Quando a UI mudar, atualizar o canvas primeiro e depois o código (firmware ou app).

### Telas

| # | Arquivo no canvas | Tela | Observações |
|---|---|---|---|
| 01 | `Main.dc.html` | Boot | Logo + barra de progresso "Lendo cartão SD… 642 / 1025 registros" |
| 02 | `ErroSD.dc.html` | Sem cartão SD | Botão "Tentar de novo" (44 px). Pede FAT32 |
| 03 | `Menu.dc.html` | Menu | Barra de capturados (312/1025), tile grande Lista + Saves, Hall da Fama (com a contagem no canto), Progresso, Ajustes; botão "Vitrine" no topo. Buscar e Tipos ficam só no cabeçalho da Lista/Grade. Sem indicador de SD (sem cartão o aparelho não passa do Boot) |
| 04 | `Progresso.dc.html` | Progresso | Anel de % capturados + barras por geração (I–IX) |
| 05 | `Ajustes.dc.html` | Ajustes (raiz) | Uma linha "›" por grupo, com resumo: Tela e som, Navegação, Modo vitrine, Sistema. Versão no cabeçalho |
| 05a | `AjustesTela.dc.html` | Ajustes › Tela e som | Brilho de dia e brilho de noite (valem pra tudo, Vitrine também; prévia enquanto arrasta), pôr do sol (só leitura: hora de hoje, "defina no app" ou "sem hora"), apagar tela, som ao tocar. 5 linhas = 1 página |
| 05f | `AjustesNavegacao.dc.html` | Ajustes › Navegação | Catálogo em Lista/Grade, ao ligar Menu/Vitrine (segmentados na linha) |
| 05b | `AjustesVitrine.dc.html` | Ajustes › Modo vitrine | Troca a cada, ordem, Mostrar › (Escolha: capturados/vistos/todos/meu time) + resumo do ciclo; ▶ no cabeçalho inicia a vitrine |
| 05c | `AjustesSistema.dc.html` | Ajustes › Sistema | Wi-Fi › (rede atual), Idioma › (Escolha), Cartão SD (registros, saves, "Recarregar", 52 px) |
| 05g | `AjustesEscolha.dc.html` | Ajustes › Escolha | Genérica: uma linha de 34 px por opção, dica muted à direita, ✓ na atual; tocar grava e volta. Pagina com 6+ (tweak mostra Idioma com 2 páginas) |
| 05d | `AjustesWiFi.dc.html` | Ajustes › Sistema › Wi-Fi | Interruptor + estado, 4 redes por página (barras de sinal, cadeado, ✓ e "Esquecer" na atual, que fica no topo), ↻ busca de novo, ⓘ abre 05d'. Rodapé de paginação sem "Ir para" |
| 05d' | `AjustesWiFiInfo.dc.html` | Wi-Fi › Conexão | cydex.local, IP, chave da API e como mandar aviso |
| 05e | `WiFiSenha.dc.html` | Wi-Fi › Senha | Campo mascarado (olho mostra), faixa com rede + estado, teclado abc/ABC/123/#+= (só ASCII). "Conectar" vira "Pronto" quando conecta |
| 06 | `Lista.dc.html` | Lista | Cabeçalho: buscar, tipos, ver em grade. Filtro Todos/Capturados/Faltando, 4 linhas por página, rodapé de paginação |
| 06b | `IrPara.dc.html` | Ir para | 9 gerações (3×3) + teclado numérico (#1–1025). Os dois levam à página que contém o número, na visão de origem (Lista ou Grade); não abrem a Ficha |
| 06c | `ListaFiltro.dc.html` | Lista com filtro | Com busca ou tipo ativo, o "Nacional" vira um chip (tocar limpa). Vale pra Grade também |
| 07 | `Grade.dc.html` | Grade | 5×2 (10 por página), sprite 48×48, rodapé de paginação |
| 08 | `Busca.dc.html` | Busca | Campo no cabeçalho, sugestões (até 3), teclado QWERTY de 40 px + modo 123. Ignora maiúsculas, acentos e pontuação; nome só casa com espécie vista; "Ver" vira filtro |
| 09 | `Tipos.dc.html` | Filtro por tipo | 18 tipos em grade 4×5, até 2 selecionados (o 3º troca o mais antigo), "Aplicar (n)" |
| 10 | `Detalhe.dc.html` | Ficha: Info | Sprite 96×96, nome, tipos, altura, peso, geração, status de captura. Se algum save tiver a espécie, 4ª linha "Onde está" ("Time · Ana" / "Caixa 3 · Ana", "+n"; linhas de 22 px), que abre o save na aba certa |
| 11 | `DetalheStatus.dc.html` | Ficha: Status | 6 barras (PS, Atq, Def, AtE, DfE, Vel) + total |
| 12 | `DetalheEvol.dc.html` | Ficha: Evolução | Até 3 estágios 64×64 com nível entre eles |
| 13 | `NaoVisto.dc.html` | Ficha: não visto | Tudo "???", abas Status/Evolução desativadas |
| 14 | `Vitrine.dc.html` | Modo vitrine | Carta 314×234: marca de status (14 px, sem a palavra) + nome (16 px, 1 linha, "…") + tipos abreviados; janela de 144 (borda na cor do tipo) com o sprite 2× cortado; medidas + 6 status com rótulo e valor; rodapé "#nº · Geração" + indicadores (14b). Toque sai pro Menu. Tweak `naoVisto`: só nº, nome e sprite, o resto "???" |
| 14b | `VitrineClaude.dc.html` | Vitrine com indicadores | Chips de 16 px à direita do rodapé (`PUT /status`; o "#nº · Geração" cede espaço com "…"): ícone opcional + texto + barra 28×4 opcional. "claude" = terminal + nº de sessões trabalhando (amarelo: esperando você); "quota" = "5h 20% \| 7d 45%", o quanto já foi usado (como o `/usage`), só texto, na cor da janela mais usada (muted até 50%, amarelo até 80%, accent) |
| 15 | `Aviso.dc.html` | Aviso | Faixa de 56 px por cima de qualquer tela com o aviso mais importante: barra na cor do aviso, ícone, título, "subtítulo · mensagem" (2 linhas), "+n" dos outros. Toque fecha |
| 15b | `VitrineAviso.dc.html` | Avisos na Vitrine | Até 3 balões compactos de 162×50 (ícone + título, subtítulo) empilhados à direita da janela do sprite, a partir do topo dela; o mais importante em cima e só ele com a ponta apontando pro sprite; toque num balão fecha só ele |
| 15c | `VitrinePermissao.dc.html` | Vitrine: responder permissão | Aviso com `actions` ganha "›" no balão; tocar abre a folha 162×176 no lugar dos balões (ponta na mesma altura): título (2 linhas), pasta, mensagem (2 linhas), Negar / Permitir (40 px) e "ou responda no terminal". Permitir fica tracejado 1 s antes de aceitar toque. Tocar fora fecha sem responder; com a folha aberta a espécie não troca; fecha sozinha se o pedido sumir/mudar ou em 60 s |
| 16 | `Saves.dc.html` | Saves | Cards de 50 px (3 por página, rodapé só "pág X de Y"): faixa na cor do jogo, treinador + gênero, chip do jogo, "tempo · dex x/386", 8 insígnias. Não reconhecido: tracejado, sem toque |
| 16a | `SavesVazio.dc.html` | Saves: nenhum | Cartucho tracejado + como mandar saves |
| 17 | `SaveResumo.dc.html` | Save: Resumo | Abas Resumo/Time/Caixas (uma tela, painéis). Avatar com a inicial, ID · arquivo, tempo, insígnias, dex deste save (visto por baixo do capturado), dinheiro, Hall da Fama (abre o 19 filtrado), brilhantes. ‹ › trocam de save |
| 17a | `SaveTime.dc.html` | Save: Time | 3×2 com sprite 48, apelido, Nv, ★ e barra de PS (verde > 50%, amarelo > 20%, accent); vaga vazia tracejada |
| 17b | `SaveCaixas.dc.html` | Save: Caixas | 6×5 células de 28 com ícone 24 + painel do selecionado (‹ › da caixa, sprite 48, "Abrir ›"). No cabeçalho, "N nas caixas" no lugar de ‹ › |
| 18 | `Membro.dc.html` | Membro | Abas Info (selo ★/Nv, espécie que abre a Ficha, tipos, natureza, item, treinador) / Status (valores reais; o que a natureza sobe em accent, o que desce em azul) / Golpes (tipo, nome, PP atual/máx) |
| 19 | `HallDaFama.dc.html` | Hall da Fama | Times de todos os saves, cards de 50 px com "#n", treinador · jogo, "1ª vitória" no #1 ou "Nv máx", 6 ícones 24. Chip do filtro por save (Escolha 05g) |
| 19a | `HallTime.dc.html` | Hall da Fama: time | "#n · Ana" + chip, faixa (mais recente / 1ª vitória com Xh / nº), 3×2 cards que abrem a Ficha; ‹ › pro time vizinho |

## Hardware

- **ESP32-2432S028 (sem o "R")**, a mesma placa física do PokEsp (`~/Development/PokEsp`). Display 320×240 com controlador **ST7789** (a serigrafia sugere ILI9341, mas não é), usado em **paisagem** (`setRotation(1)`). Precisa de `TFT_RGB_ORDER TFT_BGR` + `TFT_INVERSION_OFF` (`include/tft_setup.h`). **Sem PSRAM.**
- Três barramentos separados, sem mutex de SPI: display na HSPI (12/13/14/15), toque XPT2046 resistivo por **bit-bang** próprio (25/32/39/33; o touch embutido do TFT_eSPI não serve) e microSD na VSPI (18/19/23/5). Pinos só em `include/board_pins.h`. Backlight por PWM (ledc) no GPIO 21. Alto-falante no conector P4 (GPIO 26).
- Lições do PokEsp (não repetir):
  - A pilha da loopTask fica em **32 KB** (`getArduinoLoopTaskStackSize`). A cadeia mais funda medida com `-fstack-usage` é o parse do save Gen3 (~12,6 KB) + LVGL. Era 64 KB (número do PokEsp), mas a pilha sai do heap e o Wi-Fi precisa dela. Struct grande em variável local na cadeia LVGL → parser → SD estoura. Medir com `-fstack-usage`, não estimar.
  - Sem bloco contíguo grande no heap. Ler arquivos grandes do SD em pedaços (`sd_read_at`).
  - Não abrir arquivo dentro do callback de `sd_list_files`. Listar primeiro, abrir depois.
- **Orçamento de memória com Wi-Fi** (medido na placa; o Wi-Fi conectado leva ~70 KB de heap e pica o resto):
  - O heap do ESP32 é em regiões separadas e o maior bloco contíguo no boot é de ~47 KB. Nada pode pedir mais que isso (o pool do LVGL no heap travava o `lv_init`).
  - O pool do LVGL é estático e tem 32 KB (pico medido ~10 KB, rodando todas as telas). O buffer do display (15 KB) e os nomes (16 KB) saem de **um** bloco de heap (`util/early_alloc`), porque pedidos separados ocupavam as regiões pequenas que sobram pros sprites.
  - Os sprites saem de uma área fixa de 46 KB (`sprite_reserve()`, 40 vagas de 1152 B: 24 = 1 vaga, 48 = 4, 64 = 8, 96 = 16). O pior caso de cada tela cabe (Grade 10×48, Ficha 96 + 3×64, Vitrine 96, Caixas 30×24 + 48) e as telas nunca coexistem. Com o Wi-Fi ligado, sprites soltos no `malloc` falhavam (os 3 últimos da Grade, o 96 da Ficha). Sem vaga, cai no `malloc`.
  - As telas de Saves e do Hall da Fama guardam time/caixa decodificados e a lista do Hall em vagas **emprestadas** dessa área (`sprite_arena_borrow`, via `saves_screen_mem_take`): o Save pede 6 vagas e as Caixas usam as outras 34. A RAM estática não comporta isso (o link estourava em 26 KB).
  - O parser de save não tem buffer estático: usa o `SaveReader::scratch` de 4 KB, um só no firmware (`save_scratch()`), dividido por boot, API e telas (mesma task).
  - O SD é montado no setup **antes** do Wi-Fi, com `max_files` 3 (o FATFS pede ~4 KB por arquivo num bloco só). O `sd_init` não remonta se já estiver montado (Recarregar com o Wi-Fi ligado falhava).
  - Ordem do setup: `early_alloc_init` → display/LVGL → reservas (nomes, sprite 96) → SD → Wi-Fi. Mudou algo grande? Compilar com `-D LOG_LEVEL=2` e olhar o `[D][mem]` (a cada 10 s) e o `maior_bloco` depois de conectar (hoje ~57 KB livres além das reservas, pilha com ~29 KB livres).
- Print da tela: comando `shot` na serial (`serial_cmd_tick` no `main.cpp`) → `display_capture_to_serial()` invalida a tela, `lv_refr_now` e o `flush_cb` também manda cada faixa (`"CYSA"` + x,y,w,h + RGB565 LE + soma u16, fim `"CYSE"`). `tools/cydex_screenshot.py` monta o PNG (2×) em `docs/img/`. O CH340/macOS perde um byte de vez em quando: faixa com soma errada é descartada e só aquelas linhas são pedidas de novo (`shot <y0> <y1>`). ~14 s a 115200 (na Vitrine, uma troca entre o pedido e o reenvio deixa degrau). No macOS abrir a porta reinicia a placa mesmo com DTR/RTS em falso: manter o script aberto enquanto navega.
- Stack: **PlatformIO + Arduino**, LVGL 8.3.11, TFT_eSPI 2.5.43. Comandos: `pio run -e cyd -t upload`, `pio device monitor`, `pio test -e native_test` (só `lib/core`, sem Arduino).
- Processador e barramento são o gargalo: uma tela cheia (~150 KB em RGB565) leva ~30 ms só de SPI a 40 MHz, e o painel não tem sincronia (rasga em animação).
- UI em **LVGL**.

## Regras de UI (decididas, não mudar sem motivo)

- **Sem scroll.** Nada de arrastar/inércia. Listas paginam com botões ◀ ▶ (72 px de largura) no rodapé; segurar a seta repete a cada ~250 ms e acelera pra ~90 ms depois de ~8 passos. Tocar no indicador do meio abre "Ir para". Se algum lugar precisar de scroll, desligar `LV_OBJ_FLAG_SCROLL_MOMENTUM` e `LV_OBJ_FLAG_SCROLL_ELASTIC` e usar `lv_obj_scroll_to_y` sem animação.
- **Um toque = um redesenho.** Sem animação de transição. Carregar os sprites da página de uma vez ao trocar de página.
- **Alvos de toque ≥ 32 px** (teclas 40 px, botão principal 40–44 px).
- Medidas fixas: cabeçalho 32 px (36 na busca), abas e rodapé de paginação 38 px, linhas de lista 34 px.
- **Tema escuro** de propósito (o TN do CYD lava fundo claro fora do ângulo).

### Tokens

| Token | Valor | Uso |
|---|---|---|
| bg | `#0F1317` | fundo |
| surface | `#1A2027` | cabeçalho, cards, tiles |
| surface-2 | `#232B34` | trilhos de barra, teclas, chips neutros |
| line | `#2C3540` | bordas; divisória de linha `#1F262E` |
| text | `#ECEFF2` | texto principal |
| muted | `#93A0AD` | texto secundário |
| faint | `#4A5562` / `#3A4552` | desconhecido, desativado, tracejado de sprite |
| accent | `#FF5A4E` | capturado, ativo, botão principal (texto sobre ele: `#0F1317`) |
| accent-soft | `#2A1A1A` | fundo do selo "Capturado" da Ficha |
| surface-dim | `#161B21` | moldura do sprite de espécie não vista |

Cores de tipo (chip preenchido com texto `#0F1317`): Normal `#C9C2B4`, Fogo `#FF8A4C`, Água `#5AA8FF`, Grama `#6FCF6A`, Elétrico `#F5D547`, Gelo `#8FE0E6`, Lutador `#E8836B`, Veneno `#C78BE8`, Terra `#E0B970`, Voador `#A8B8FF`, Psíquico `#FF7FB0`, Inseto `#B5D45A`, Pedra `#CBB77A`, Fantasma `#A99CE0`, Dragão `#9C8CFF`, Sombrio `#A89A8C`, Aço `#B7C6D3`, Fada `#FFB3D9`.

Status de espécie: círculo cheio accent com check = **capturado**; contorno muted = **visto**; tracejado faint = **desconhecido** (nome vira `???`).

### Fonte

- Montserrat nos tamanhos 12 / 14 / 16 / 20 / 28 px (pesos 500/600/700 no layout).
- As fontes embutidas do LVGL só têm ASCII. As do CYDex são geradas por `tools/gen_fonts.mjs` (`cd tools && npm install && npm run fonts`): Latin-1 mais `… ‹ › • – — “ ” −`, 4 bpp, em `src/ui/fonts/` com o nome `font_<px>_<peso>`. A lista de combinações fica no script; se uma tela pedir outra combinação, acrescentar lá e regenerar. A fonte padrão do LVGL é `font_14_500`.

### Ícones

- Os SVGs do canvas viram imagens A8 em `src/ui/icons/` com `tools/gen_icons.mjs` (`npm run icons`). Cada imagem tem um tamanho fixo e é pintada em tempo de execução com `ui_icon(parent, &icon_x, cor)`. Ícone novo: copiar o `<path>` do canvas pro script.

### Idiomas

- **Nenhum texto de interface fica escrito direto no código das telas.** Tudo passa por `tr(STR_*)` (`lib/core/src/i18n/i18n.h`). Exceções: o nome "CYDex" e os nomes das espécies (que vêm do SD).
- Um arquivo por idioma em `lib/core/src/i18n/lang/` (`pt_br.c` é a referência). Eles são C, não C++, por causa do inicializador designado `[STR_X] = "..."`.
- Texto novo: criar a chave em `i18n.h` e escrever em todos os idiomas. Idioma novo: copiar `pt_br.c`, traduzir e registrar em `LangId` e `kLangs` (`i18n.c`). O `test/test_i18n` falha se faltar texto, se os `%` divergirem do pt-BR ou se aparecer caractere sem glifo na fonte.
- A fonte cobre só Latin-1: pt, en, es, fr, de e it cabem. ja, ko e zh exigem outras fontes.
- O idioma fica salvo na NVS (`data/Settings`, chave `lang`). O padrão de fábrica vem de `-D CYDEX_DEFAULT_LANG=\"pt-BR\"`. Depois de trocar, `nav_reload()` remonta a tela. A escolha fica em Ajustes › Sistema (05c).
- Os nomes das espécies saem no idioma de `tools/build_sd.py --names-lang` (padrão `en`; a PokeAPI não tem pt-BR, e os nomes oficiais em português são os do inglês).

### Código da UI

- `src/ui/theme.h` traz os tokens e as medidas fixas. `src/ui/widgets.*` tem as peças comuns (tela, label, ícone, logo, barra, cabeçalho). `src/ui/nav.*` cuida da troca de tela: uma por vez, sem animação. A anterior é apagada **antes** de a próxima ser montada, porque as telas guardam estado em variáveis estáticas: com as duas vivas, ir de Ficha para Ficha liberava o sprite da nova e vazava o da antiga.
- Uma tela do canvas = um arquivo em `src/ui/screens/`. O que a tela aloca (sprites, timers) é liberado no `LV_EVENT_DELETE` da própria tela.
- `ui/pager_bar.*` é o rodapé de paginação compartilhado (◀ ▶ com repetição ao segurar, toque no meio abre Ir para). Ele avisa `holding=true` enquanto o dedo está na seta. A Grade usa isso pra só ler sprites quando a página para.
- `ui/catalog.*` guarda o estado entre telas: filtro, âncora (1º número visível, que mantém a posição ao trocar Lista ↔ Grade e ao voltar da Ficha) e a visão de origem. A lista filtrada fica num array estático de 2 KB e só é refeita quando o filtro muda (`catalog_invalidate()`).
- A Ficha (`screens/detail_screen.cpp`) é uma tela só para as 4 artboards (10–13): cada aba é um painel que se esconde ou aparece, sem reler o SD. O registro é lido ao abrir, o sprite 96 é alocado primeiro (é o maior bloco) e os 64 da Evolução só na primeira vez que a aba abre. ‹ › usam `catalog_neighbors` na lista filtrada e mantêm a aba (`CatalogState::tab`). As regras que o canvas não mostra estão na nota "Ficha: regras" do canvas.
- Busca e Tipos voltam pra tela de onde vieram (`nav_previous()`). As regras completas estão na nota "Busca e Tipos" do canvas. O `catalog_header()` monta o cabeçalho da Lista e da Grade, com o chip quando `CatalogQuery::refined()`.
- `ui/keyboard.*` é o teclado de 40 px da Busca (layout `Search`) e da Senha do Wi-Fi (layout `Text`). Um por vez, estado em estáticos.
- Ajustes (05, 05a, 05b, 05c, 05f, 05g): árvore de menus, uma tela por artboard. ‹ sobe um nível. Os valores ficam em `AppSettings` (`data/Settings`, NVS). As peças comuns (slider, interruptor, segmentado, "− valor +", linha "›") estão em `ui/settings_widgets.*`. Os sliders aplicam na hora e só gravam ao soltar. O app do computador também muda os Ajustes (`PUT /settings`, `ui/settings_remote.*`): grava, reaplica o brilho (`power_settings_changed()`, que respeita a prévia de um slider) e remonta a tela aberta se ela mostra o que mudou (Ajustes; Vitrine com troca/ordem/mostrar; qualquer uma no idioma novo, menos Busca, Senha do Wi-Fi e Escolha).
- Cada tela de Ajustes descreve as linhas numa tabela `SettingsRow` (altura + função que monta) e chama `settings_page()` (`ui/settings_page.*`): se tudo cabe nos 208 px, uma página sem rodapé; senão páginas de 170 px com o `pager_bar` (só "pág X de Y"). A quebra é `RowPages` (`logic/RowPages`, testada). Linha nova num grupo = uma entrada na tabela; a paginação vem sozinha.
- Escolha com 2 opções curtas fica na linha (segmentado). Com 3+ ou nomes longos, a linha "›" chama `settings_choice_open(ChoiceSpec)` (05g): título, `label`/`hint` por índice, `pick` grava, `back` é a tela que volta.
- `ui/power.*` cuida do brilho, do "apagar tela" e do som ao tocar. As telas pedem brilho por `power_use_normal_level()` (escolhe o de dia ou o de noite; vale pra todas, Vitrine também), nunca direto no backlight; `power_set_level()` é só a prévia dos sliders. Noite = entre o pôr e o nascer do sol (`logic/Sun`, em UTC pela lat/lon de `PUT /location`), conferido a cada 60 s no `power_tick()`. Sem localidade ou sem hora (o relógio só acerta pelo NTP com o Wi-Fi; antes de 2024 = sem hora), vale o de dia. O 1º toque com a tela apagada só acende. Na Vitrine a tela nunca apaga. O som é um clique de 12 ms no alto-falante (GPIO 26, canal 2 do ledc, porque os canais 0 e 1 dividem o timer do backlight).
- A versão ("CYDex v0.1") fica em `include/version.h`.
- A lógica sem LVGL fica em `lib/core/src/logic/` (`Pager`, `HoldRepeat`, `RowPages`, `Catalog`, `NumberInput`, `Format`, `Search`, `Showcase`, `Sun`, `TextFit`, `WakeLease`, `SettingsPatch`), `notify/` (`NoticeJson`, `Notice`, `NoticeBoard`) e `net/` (`HttpRequest`, `WifiList`), e tem testes nativos.
- O LVGL 8 não tem `margin`. Usar um invólucro com padding.
- Sem tema padrão do LVGL (`LV_USE_THEME_DEFAULT 0`): todo widget é estilizado à mão a partir dos tokens.

### Sprites

- Vêm do SD. Base 96×96.
- Ficha: 96×96 (1×). Evolução: 64×64. Grade: 48×48 (metade). Vitrine: 96 em 2× (vizinho mais próximo) numa janela de 144, ou seja, 72 px do original.
- Usar só escalas inteiras ou metade; escala fracionária borra pixel art e custa mais.
- Corte da Vitrine no CYD, a partir do `sprites_96.bin` (sem arquivo próprio no SD): `showcase_opaque_box` acha a caixa opaca e `showcase_crop_origin` põe a janela de 72 no meio dela, presa no quadro; mais alta que 72, ancora embaixo (mantém os pés, corta o topo); mais larga, corta as laterais por igual. O `lv_img` fica com pivô (0,0), zoom 2× e posição −2×origem; a janela recorta (`clip_corner`) e desenha a borda por cima (`border_post`).

## Modo vitrine

- Sempre ligado, mostra uma espécie por vez.
- Layout "carta" (cartão 314×234 em x=3, y=3, padding 3/5): cabeçalho de 22 px com a marca de status (`ui_status_mark` 14 px, antes do nome; no "Meu time" não tem) e o nome em 16 px numa linha (`ui_label_clamp`, "…") e os tipos abreviados à direita (`species_type_abbr`, 3 letras, `STR_TYPE_ABBR_*`); o nome fica com a largura que os chips deixam. Corpo: janela de 148×148 (borda 2 na cor do tipo 1 misturada 50% com o fundo; com 2 tipos, esquerda/baixo no tipo 1 e topo/direita no tipo 2, por um anel sobreposto com a borda inteira do tipo 2 recortada na diagonal por `lv_draw_mask_line` (borda parcial com raio não desenha a curva no LVGL 8.3); faint na não vista) e, à direita, medidas + status. Rodapé de 18 px + borda: "#nº · Geração X" (`STR_SC_FOOTER_FMT`, cede espaço com "…") e os indicadores. Sem sombra no chão nem total.
- Ajustes (salvos na NVS com `Preferences`, lidos ao entrar):
  - troca a cada: 30 s, 1, 2, 5 ou 15 min (padrão 1 min)
  - ordem: sequencial ou aleatória. A aleatória não repete até mostrar todas ("saco": `showcase_pick`, bits por nº, só na RAM; a 1ª da rodada nova não é a última da anterior). Sortear com reposição repetia muito (paradoxo do aniversário).
  - mostrar: capturados, vistos ou todos (as 1025; `showcase_source`, chave `sc_src`, lê a antiga `sc_all` se faltar). Não vista mostra só nº, nome e sprite; tipos, medidas e status ficam "???", sombra neutra
  - brilho: o mesmo das outras telas (Brilho de dia 70% e de noite 30%, Ajustes › Tela e som). O brilho próprio da Vitrine saiu; as chaves `sc_bright`/`sc_bright_n` são apagadas no boot
  - ao ligar: Menu ou Vitrine (padrão Menu), em Ajustes › Navegação
- Redesenha a tela inteira só na troca; entre trocas nada muda.
- Desloca o layout 1–2 px a cada troca (evitar retenção de imagem no LCD).
- Status: 6 linhas de 14 px (PS, Atq, Def, AtE, DfE, Vel): rótulo muted, valor e barra de 4 px na escala até 150, ≥ 60 em accent. Não vista: valores "???" e trilhos vazios.

## Dados

- Catálogo nacional: 1025 espécies, 9 gerações (I 1–151, II 152–251, III 252–386, IV 387–493, V 494–649, VI 650–721, VII 722–809, VIII 810–905, IX 906–1025).
- Tudo vem do cartão SD (FAT32). Espécies e sprites são gerados no PC por `tools/build_sd.py` (PokeAPI). Status visto/capturado vem dos saves do usuário.

```
/cydex/species.bin          cabeçalho de 16 B ("CYDX", versão, count) + 1025 registros fixos de 64 B (seek em 16+(n-1)*64)
/cydex/sprites_96.bin       todos os 96×96 num arquivo: cabeçalho "CYSP" + registros fixos (lv_img_header + RGB565, chroma key 0x00FF00)
/cydex/sprites_64.bin       64×64 já reduzido no PC (Evolução)
/cydex/sprites_48.bin       48×48 já reduzido no PC (Grade, Time, Hall da Fama)
/cydex/sprites_24.bin       24×24 (Caixas e lista do Hall da Fama)
/cydex/moves.bin            golpes 1..354: nome, tipo, PP base ("CYNT", dex/NameTable.h)
/cydex/items_g3.bin         nome do item pelo índice do Gen3 (game_indices da PokeAPI)
/cydex/saves.cache          gerado pelo CYD junto do status.cache: um resumo fixo por save (SavesCache.h)
/cydex/status.cache         gerado pelo CYD: assinatura dos saves + bits visto/capturado
/saves/*.sav, *.srm         saves (hoje só Gen3; uma geração nova é um parser novo em lib/core/saves)
```

- Os layouts ficam em `lib/core/src/dex/SpeciesRecord.h`, `SpriteFile.h` e `NameTable.h`, que são a fonte da verdade e devem bater com o `tools/build_sd.py`. O byte 39 do `species.bin` é a curva de crescimento (0 = desconhecida, SD antigo: o nível de quem está na caixa vira "Nv ?").
- **Nunca uma pasta com milhares de arquivos.** No FAT32 cada abertura varre a pasta inteira: com 1025 sprites soltos, cada um levava ~560 ms. Use arquivo único com registros fixos e `seek`. O `sd_open_read` reaproveita o arquivo se o caminho for o mesmo. SPI do SD a 20 MHz (o padrão do `SD.begin` é 4 MHz).
- Os saves são lidos **só no boot** (ou em "Cartão SD › recarregar"). O resultado (`DexStatus`, 2×129 B) fica em RAM. Se a assinatura dos saves (nome, tamanho, data) não mudou, o boot usa o `status.cache` e pula o parse.
- Na RAM ficam só os nomes (para Lista e Busca). O registro completo é lido do SD quando a ficha abre.

### Saves e Hall da Fama

- Modelo genérico em `lib/core/saves/` (`SaveMon`, `SaveSummary`/`SaveHead`, `SavesCache`); cada geração só preenche os mesmos tipos pelo `SaveFormat` (`open`, `summarize`, `read_party`, `read_box`, `read_hof_team`). Gen3 em `saves/gen3/` (`Gen3Pk3` decifra, `Gen3Species` índice interno → nacional, `Gen3Save` lê aos pedaços seguindo os IDs das seções; offsets conferidos no PKHeX). Nível e status de quem está na caixa: `logic/MonStats` com a base e a curva do `species.bin`.
- O parse pesado (time, 14 caixas, Hall da Fama) roda só junto do da dex, quando a assinatura dos saves muda, e grava o `saves.cache` (registro fixo por save: resumo, time, hashes dos times do Hall, "onde está" com 1 byte por espécie até 386). Listas, Menu, "Onde está" e "Meu time" leem dele com seek (`data/SaveStore`); só o time, uma caixa e um time do Hall abrem o save.
- O Gen3 não guarda data no Hall da Fama: só a ordem (0 = o mais antigo) e o tempo de jogo da 1ª vitória (game stat 1). O mesmo time em dois saves (mesmo PID + ID nas 6 vagas) entra uma vez.
- Vitrine › Mostrar "Meu time" (`ShowcaseSource::Team`): o time de todos os saves; o saco sorteia por posição. Apelido + Nv no nome, "#nº · Espécie", o save e ★ no rodapé; status da espécie.

## Wi-Fi e avisos

- **Sem servidor no PC.** O próprio CYD serve a API em `http://cydex.local` (porta 80, mDNS). O firmware não sabe nada de Claude: qualquer coisa (CI, cron, Home Assistant, script) manda aviso com `curl -4`. Teste com `tools/cydex_notify.sh`. Sempre `-4`: o CYD só tem IPv4 e o macOS espera 5 s pela resposta AAAA do mDNS. Com IP fixo, o cliente para quando o DHCP troca o endereço.
- Rotas (todas menos `GET /` pedem `Authorization: Bearer <chave>`):
  - `POST /notices` cria ou substitui → **201 `{"id":"…"}`**; 400 `{"error":"<campo>"}` (ou `"json"`); **413 com corpo > 4 KB**.
  - `DELETE /notices/<id>` fecha (`?tag=x`: só se a tag bater); `DELETE /notices` fecha todos. **204** sempre (idempotente). Fechar não acende a tela nem toca nada.
  - `GET /notices` lista os abertos (`id`, `tag`, `title`, `type`, `age` em s), pra depuração. `GET /` dá nome, versão, `api` (2 = tem `/saves` e `/dex`; 3 = tem `/location`; 4 = tem `/status` e resposta nos avisos; 5 = tem `/wake?ttl`; 6 = tem `/settings`) e `rssi`.
  - `GET /settings` (api 6): `{"bright","bright_n","off_min","sound","grid","sc_int","sc_rand","sc_src","sc_boot","lang"}` (os nomes das chaves da NVS) + as opções que a UI aceita (`off_opts`, `sc_int_opts`, `langs` com `code`/`name`). `PUT /settings?<campo>=<valor>[&…]` (sem corpo, cabe nos 47 B da query; o app manda um campo por vez) → **204**; 400 `{"error":"<campo>"|"query"}`. Validação igual à da UI (bright 10–100, bright_n 5–100, off_min e sc_int só os valores da lista, sc_src 0–3, booleanos 0/1/true/false, lang um código do firmware), tudo ou nada, em `logic/SettingsPatch` (testado). Responde antes de aplicar (o `nav_reload` demora).
  - `GET /notices/<id>` → `{"id","rev","answer"}`: `answer` null = aberto; `"allow"`/`"deny"` = respondido na folha 15c (a resposta fica 2 min); 404 = fechado ou expirado. O `POST /notices` devolve o `rev` (revisão do aviso que ficou, 0 se não entrou): o mesmo id é reaproveitado por chat, e quem espera só aceita a resposta da sua revisão.
  - `PUT /status/<id>` (id até 16) cria ou troca um indicador do rodapé da Vitrine (14b; `notify/Status.h`): `text` (obrigatório, até 23 B; ~20 caracteres), `icon` (os dos avisos + `terminal`; desconhecido = sem ícone), `color` (ícone e barra, padrão muted), `text_color` (padrão text), `bar` (0–100), `ttl` (s, 0 = até `DELETE`, máx 24 h) → **204**; 400 `{"error":"<campo>"}`; **409** `{"error":"full"}` com 3. O mesmo conteúdo só renova o prazo (a Vitrine não redesenha). `DELETE /status/<id>` e `DELETE /status` → 204; `GET /status` lista (`id`, `text`, `age`).
  - `GET /saves` lista (`name`, `size`, `mtime`, `format` = parser que reconheceu ou `null`; até 16, sem os `._*` do macOS). `PUT /saves/<nome>` grava (**201**; 400 `name`; 411; 413 > 1 MB; 507). `DELETE /saves/<nome>` **204**. `POST /saves/reload` = Ajustes › Sistema › Recarregar sem passar pelo Boot (`status_store_load(true)`, `catalog_invalidate()`, `nav_reload()`) → 200 `{"saves","parsed","seen","caught"}`. Upload não relê sozinho.
  - `GET /dex`: `{"count","seen","caught"}`, bits em hex (bit = n − 1, como o `DexStatus`).
  - `PUT /location?lat=<graus>&lon=<graus>&tz=<min do UTC>` (sem corpo, cabe nos 47 B da query) grava na NVS (`loc_lat`/`loc_lon`/`loc_tz`) → **204**; 400 `{"error":"lat"|"lon"|"tz"}` (lat ±90, lon ±180, tz inteiro ±840). `DELETE /location` → 204 (volta ao brilho de dia). `GET /location`: `{"lat","lon","tz","night","rise","set"}` (`rise`/`set` "HH:MM" no `tz`; `null` sem hora ou no dia/noite polar; `{}` sem localidade). O CYD calcula o sol sozinho; o `tz` é só pra mostrar. Quem manda é o app `desktop/`.
- Corpo do `POST /notices` (campos desconhecidos são ignorados; null = ausente):
  - `title` (**obrigatório**, ~28 caracteres na faixa), `subtitle` (a origem, 31 B), `message` (2 linhas, 111 B). Passam por `text_fit()`.
  - `id` (até 40, `[A-Za-z0-9._:-]`): o mesmo id **substitui** o aberto (é o estado atual de uma coisa: um chat, um build). Sem id, o CYD gera `n<seq>` e devolve. `tag` (até 16, mesmo alfabeto): fechar seletivo.
  - `type`: preset de cor, ícone e prioridade. `info` (padrão; muted, info, low), `success` (`#6FCF6A`, check, low), `warning` (`#FF8A4C`, alert, normal), `waiting` (`#F5D547`, clock, normal), `question` (accent, shield, high), `error` (accent, alert, high). `priority` (`high`/`normal`/`low`), `color` (`#RRGGBB`) e `icon` (`info`, `check`, `alert`, `shield`, `clock`, `bell`) sobrescrevem. Ícone desconhecido cai no do preset (ícone novo não quebra firmware antigo); outro valor desconhecido é 400.
  - `actions` (lista: `"allow"`, `"deny"`; nomes desconhecidos ficam de fora): o balão ganha "›" e abre a folha 15c em vez de fechar. Só na Vitrine; a faixa das outras telas continua fechando no toque. Aviso com `actions` nunca conta como repetido. A resposta só vale com o aviso aberto, na mesma revisão, antes do `timeout` e se ele aceitar aquela ação (`NoticeBoard::answer`, como o `ApprovalStore.Decide` do phonebox).
  - `timeout` (inteiro em s, 0–86400; 0 = até tocar ou `DELETE`), `sound` e `wake` (bool, padrão `true`; `wake: false` + `sound: false` = aviso silencioso, nem fura o `/sleep`).
- Claude Code: o hook (`tools/claude-hooks.example.json`, `type: "command"`, `async: true`; `tools/cydex_install_hooks.sh <settings.json>` junta no settings sem mexer nos outros hooks, troca o caminho pelo absoluto e pode rodar de novo; `--remove` tira, `--dry-run` só mostra) chama `tools/cydex_claude.sh`, que traduz o evento com `jq` e chama a API. `PermissionRequest` (async, só avisa) → `question` (tag `permission`) com o título dizendo o que o Claude quer ("Rodar git", "Editar nav.cpp", "Abrir github.com", "MCP Gmail", "Pergunta"; o balão da Vitrine só mostra título e subtítulo). Do Bash sai só o 1º programa, depois de `cd`/`sudo`/`env`/`VAR=` (regra do `ClaudeTranscript.Program` do phonebox): com `$(`, `${`, crase, barra invertida ou aspas abertas sai "Rodar comando". Arquivo = só o nome, URL = só o host. A mensagem é o `description` do Bash ou a pergunta do `AskUserQuestion`. O `Notification` `permission_prompt` ainda é tratado (settings antigos), mas saiu do exemplo: os dois juntos sobrescreviam o título detalhado. Sessões de fundo (cwd em `.claude-mem/observer-sessions/` ou `CodexBar/ClaudeProbe`) são ignoradas. `CYDEX_DRY=1` só imprime. `Stop` → `success` (tag `done`, começo do `last_assistant_message`, cortado em 400). `UserPromptSubmit`/`SessionEnd` → `DELETE /notices/<session_id>`; `PostToolBatch`/`PostToolUse`/`PostToolUseFailure` → `DELETE …?tag=permission` (aprovou). O id é o `session_id` (um aviso por chat; responder num chat não apaga o de outro). O `idle_prompt` não é usado: dispara ~60 s depois de todo `Stop` e só substituía o Terminou. O título é só o estado (Permissão, Terminou) e o subtítulo é o nome da pasta do `cwd`. Os títulos ficam no script (`CYDEX_LANG=en` pro inglês). O script nunca manda o texto do prompt. Não dá pra usar o `type: "http"` do Claude Code (o problema do IPv6 acima).
- Indicador "claude" (14b): a cada evento o script conta as sessões vivas em `~/.claude/sessions/<pid>.json` (`kill -0` no pid; só `kind` interactive, sem as de fundo) e manda `PUT /status/claude` (terminal + nº das que trabalham; amarelo `#F5D547` com o nº das que esperam, por `waitingFor` ou pelo evento) ou `DELETE` sem nenhuma. A sessão do evento usa o estado do evento (o arquivo ainda pode estar velho). `ttl` 30 min cobre um crash sem `SessionEnd`. O mesmo estado não é reenviado por 10 min (`~/.cydex.claude-chip`).
- Aprovar pelo CYD (15c), **desligado de fábrica**: `cydex_install_hooks.sh --approve` grava o `PermissionRequest` síncrono (`timeout` 300) chamando o script com `--approve` (argumento e não variável do `~/.cydex`: num hook async a decisão seria ignorada). Só espera se o teclado estiver parado há `CYDEX_APPROVE_IDLE` s (padrão 60, `ioreg HIDIdleTime`; 0 = sempre): aí o aviso vai com `actions` e `timeout` = `CYDEX_APPROVE_WAIT` (240), e o script pergunta `GET /notices/<id>` a cada 1 s (o servidor é de um cliente por vez: nada de long-poll). Allow/deny vira `hookSpecificOutput.decision.behavior` (deny com `message`). Sem resposta, 404, `rev` diferente ou você de volta ao teclado: não imprime nada e o terminal pergunta. Morto pela CLI: `DELETE ?tag=permission`. Sem estar longe, avisa em segundo plano (o terminal não espera o CYD). A documentação não diz se o terminal mostra a pergunta enquanto o hook síncrono roda; por isso o portão de inatividade. Com isso ligado, a chave da API aprova comandos.
- Cota (14b): `tools/cydex_statusline.sh` como `statusLine` lê `rate_limits.five_hour`/`seven_day` (`used_percentage`; só Pro/Max, depois da 1ª resposta) e manda `PUT /status/quota` com o que já foi usado ("5h 20% | 7d 45%", só "5h 20%" sem a de 7 dias; sem barra; `text_color` da janela mais usada: muted até 50%, amarelo `#F5D547` até 80%, accent acima; `ttl` até a de 5 h renovar) no máximo 1×/min (`~/.cydex.quota`), em segundo plano. A linha do terminal sai de `CYDEX_STATUSLINE_NEXT` (a statusline que você já tinha) ou é "pasta · modelo · 5h 37% · 7d 12%".
- A chave (8 hex) é gerada no 1º boot e fica na NVS (`hook_key`, nome antigo); aparece em Wi-Fi › ⓘ. No PC vai em `CYDEX_KEY` (ambiente ou `~/.cydex`).
- O 401 e o 413 saem no fim dos cabeçalhos, sem ler o corpo: um body grande chegando mais rápido que o loop lê enchia os buffers do TCP e o Wi-Fi perdia pacotes.
- Saves pela API:
  - Nome validado por `save_name_decode()` (`lib/core/saves/SaveName`, testado): %XX decodificado, até 64 B, ASCII sem `/ \ : * ? " < > |`, não começa com `.`/espaço, `.sav`/`.srm`. O caminho do `HttpRequest` vai até 127 B por causa disso.
  - O upload grava em fluxo em `/saves/.upload.tmp` e só renomeia no fim; corpo incompleto, 401/413 ou cliente sumido apagam o temporário (`upload_abort`).
  - A escrita é por `open()`/`write()` POSIX (`sd_open_write`), **não** `SD.open`: o FIL já vem da montagem, e o `File` do Arduino + o `FILE*` do stdio pediam ~2 KB a mais de heap. Com o Wi-Fi o maior bloco fica em ~4 KB; sem esses 2 KB o driver descartava pacote e o upload parava na 1ª janela do TCP.
  - No upload o laço lê até 64 KB por volta (pacote parado no socket segura heap que o driver precisa) e o prazo sem bytes é 10 s (retransmissão), não 3.
  - **Sem download** (`GET /saves/<nome>`): mandar centenas de KB seguidos derrubava o link (associado, RSSI bom, loop vivo, mas sem passar pacote até reiniciar; o `wifi_mgr` não recebe evento). Nem limitando a 1 KB por volta aguentou mais que ~5 downloads de 512 KB. Zeros sem tocar no SD também travavam, então é TX do Wi-Fi.
- Fechamento: o **cliente fecha primeiro**. Toda resposta tem `Content-Length` ou é `chunked` (o `GET /saves`), o servidor espera o FIN (`peer_closed()`: `recv` de 1 B com `MSG_PEEK`; o `connected()` do `WiFiClient` não vê o FIN) por até 2 s e, se não vier, fecha com RST (`SO_LINGER` 0). Quem fecha primeiro fica com o TIME_WAIT, e no ESP-IDF o PCB sai do heap.
- Ao conectar, `configTime` (NTP, UTC) pra data dos saves gravados sair certa, e o mDNS anuncia o TXT `app=cydex` (o app do computador filtra por ele).
- O corpo **não fica na RAM**: `HttpRequest` e `NoticeJson` (lib/core) leem em fluxo com memória fixa e guardam só os campos que interessam, cortados. O caminho vai até 63 B (`/notices/<id de 40>`; mais que isso é 400) e a query até 47. O servidor (`net/notice_server`) é síncrono, um cliente por vez, avançado por `wifi_tick()` no loop (mesma task do LVGL, sem mutex), até 8 KB por volta; 3 s sem bytes fecha.
- `net/wifi_mgr`: estado `Off/Idle/Connecting/Connected/Failed`, busca assíncrona, reconexão com espera de 5 s → 60 s, `WiFi.persistent(false)` (rede e senha ficam em `AppSettings`, gravadas por `settings_save_wifi()` só quando a conexão dá certo). As telas comparam `wifi_version()` num timer de 250 ms e só redesenham quando muda.
- Avisos abertos ficam no `NoticeBoard` (lib/core, dono: `notice_server`): **um por id**, até 4, ordenados por prioridade (empate: o mais antigo antes). Cheio, o novo só entra se for mais importante que o último (que sai); senão o POST dá 201 mas o aviso não aparece. O mesmo id com o mesmo título em 3 s conta uma vez. O `ui/notice_center` expira os de `timeout` (mesmo relógio, `millis()`) e só desenha quando a `version()` muda: fora da Vitrine é a faixa do 1º em `lv_layer_top()` ("+n" = os outros; sobrevive ao `nav_go`); na Vitrine são até 3 balões compactos (`showcase_show_notices()`: ícone + título na cor do aviso e, embaixo, o subtítulo ou a mensagem). Toque fecha o tocado. Aviso aceito chama `power_wake()` (se `wake`) e `speaker_chime()` (se `sound` e "Som ao tocar").
- Texto de fora passa por `text_fit()` (`logic/TextFit`): tira markdown, junta espaços, troca o que a fonte não tem (`font_has_glyph`, a mesma tabela do `test_i18n`) e corta com "…".
- `POST /sleep` e `POST /wake` (mesma chave, sem corpo, 204) apagam e acendem a tela junto com o computador: quem manda é o app `desktop/` (ver "App do computador"). No macOS o `sleepwatcher` não recebe os eventos de tela, e as notificações do `NSWorkspace` só chegam com o `NSApplication` rodando (o Tauri já roda). Apagar é só o backlight (`power_remote_sleep()`), vale até na Vitrine. Com aviso aberto não apaga; aviso novo (com `wake`) acende só o CYD e, quando o último fecha, apaga de novo. Toque com a tela apagada ou `/wake` saem do modo.
- **Rede de segurança** (api 5): `POST /wake?ttl=<s>` (1–3600; 400 `{"error":"ttl"}`; lógica em `logic/WakeLease`, testada). Sem prazo valendo, funciona como o `/wake` e começa o prazo; com prazo valendo, só estende: não acende nem conta como toque, então o "apagar tela" por inatividade continua valendo e a renovação a cada 30 s não segura a tela acesa. Sem renovação no prazo, o computador sumiu sem avisar (dormiu com o Wi-Fi caindo, app fechou, crash): `power_tick()` chama o `power_remote_sleep()`, com as mesmas regras de aviso aberto. Toque, `/sleep` e `/wake` sem ttl cancelam o prazo (quem toca está usando o CYD). `/wake` sem ttl é o de antes (app antigo). Só o `/wake` de verdade vai pro log.
- Wi-Fi vem desligado de fábrica. **Sem modem sleep** (`WiFi.setSleep(false)`): com ele o ping ia a 300–850 ms e um pacote perdido segurava o POST por 3–5 s.

## App do computador (`desktop/`)

- **Tauri 2** (Rust + Vite/TypeScript puro), bandeja em macOS, Windows e Linux. CI: `desktop.yml` (testes + clippy `-D warnings` + build do front nos 3 sistemas, a cada push em `desktop/`); `desktop-release.yml` (tag `desktop-v<X.Y.Z>` ou "Run workflow"): cria a release em rascunho, grava a versão da tag no `tauri.conf.json`/`Cargo.toml`/`package.json`, compila (macOS universal, Windows, Linux) com o `tauri-action` e publica quando os três terminam. Versão só `X.Y.Z` (o `.msi` não aceita sufixo). Instaladores sem assinatura. Rust ≥ 1.90 (`desktop/.tool-versions`).
- `src-tauri/src/`: `client.rs` (API; `PUT` repete 3×; `/sleep` e `/wake` num cliente à parte, direto no IP, com `connect_timeout` de 1,5 s), `locate.rs` (nome → IP: mdns-sd, `getaddrinfo` só IPv4 (o `-4` dos scripts), `dscacheutil` no macOS e, por último, varredura da sub-rede procurando `"name":"CYDex"` no `GET /`; uma procura por vez; ver "Achar o IP" abaixo), `power_log.rs` (registro dos envios de energia: evento, rota, resultado, motivo, tentativas, ms, IP e por onde; e as regras puras de juntar e renovar, testadas), `discovery.rs` (mDNS `_http._tcp` com TXT `app=cydex` ou instância `cydex`), `power/` (um arquivo por sistema), `config.rs` (`config.json` na pasta de config do app; na 1ª vez importa o `~/.cydex`), `save_name.rs` (mesmas regras do firmware + ajeita acento), `dex.rs` (resumo por geração), `geocode.rs` (busca de cidade no Open-Meteo, sem chave, e o fuso IANA → minutos com `chrono-tz`), `tray_text.rs` (menu da bandeja: estado, apagar/acender agora, fechar todos os avisos, abrir, sair), `lib.rs` (comandos, bandeja, status a cada 30 s; no status, com api ≥ 3, lê o `GET /location` e reenvia a cidade se lat/lon/fuso não baterem: cidade nova, CYD sem NVS ou horário de verão).
- Dormir/acordar: cada evento diz se foi a tela ou o sistema (`PowerEvent` = `power_log::Trigger`: `ScreenOff`, `SystemSleep`, `ScreenOn`, `SystemWake`, `Manual`). macOS = `NSWorkspace` pra tela dormiu/acordou e sistema acordou, e **IOKit** (`IORegisterForSystemPower`) pro "vai dormir"; Windows = janela invisível com `WM_POWERBROADCAST` + `GUID_CONSOLE_DISPLAY_STATE`; Linux = D-Bus `PrepareForSleep` (logind) e `ActiveChanged` (protetor de tela freedesktop/GNOME).
  - **O sono espera o `/sleep`** (macOS): o `NSWorkspaceWillSleepNotification` não segura nada, e o Wi-Fi caía antes do POST sair. No `kIOMessageSystemWillSleep` o handler recebe um `power::Ack` e o `IOAllowPowerChange` só sai quando o envio termina ou o teto de **5 s** passa (`SLEEP_HOLD`; nunca atrasar o sono de verdade). Largar o `Ack` também libera, então nenhum caminho deixa o sono preso. `kIOMessageCanSystemSleep` responde na hora. Se o registro no IOKit falhar, o WillSleep do NSWorkspace volta.
  - Tentativas por evento (`Plan`): vai dormir segurando o sono = 3 de até 2 s, 300 ms entre elas, dentro dos 5 s; vai dormir sem segurar (Windows, Linux) = 1 de 3 s; tela apagou = 3 (o computador segue acordado); acordar = 10 a cada 2 s (Wi-Fi e mDNS voltando); botão = 1. Para se chegar um evento mais novo (`power_seq`).
  - **Juntar e um por vez**: tela apagou + vai dormir (e acordou + tela acendeu) chegam juntos. Evento na mesma direção a menos de 1 s do anterior vira "juntado" e não manda nada (`PowerLog::join`); se for o "vai dormir", o sono espera o `/sleep` que já está no ar. Um envio no ar por vez (`power_send`, preso só durante cada tentativa): o servidor do CYD é de um cliente por vez.
  - **App Nap**: o app só tem bandeja (`Accessory`). Cada envio segura um `NSProcessInfo.beginActivityWithOptions(UserInitiatedAllowingIdleSystemSleep)` até terminar (`power::begin_activity`, vazio fora do macOS). Não impede o Mac de dormir.
  - **Rede de segurança** (api ≥ 5): o `/wake` dos eventos vai com `?ttl=90` e o status de 30 s renova (`renew_safety`, fora do registro, log em debug) enquanto o último pedido foi acender (`want_off` falso). O app **não reenvia `/sleep`**: apagaria o CYD debaixo do dedo de quem tocou nele; se o `/sleep` se perdeu, o prazo vence sozinho em 90 s. Abrir o app com o CYD apagado por inatividade acende ele (o 1º `/wake?ttl` não tem prazo valendo).
- Achar o IP (`locate.rs`): o IP achado vale 10 min **desde a última requisição que deu certo** (`touch` em toda resposta do CYD). Conexão que falha só marca como vencido (`forget`): o último IP conhecido nunca some. `/sleep` e `/wake` vão primeiro a ele, mesmo vencido (procurar leva segundos, e o mdns-sd sozinho levava 1,8 s); se não conectar, procura de novo (`resolve_fresh`) e tenta o IP novo. O mdns-sd sai da procura depois de falhar uma vez com outro caminho achando, até o app fechar (linha no log). Uma procura por vez: no boot, 4 `getaddrinfo` em paralelo pro mesmo nome.
- Registro de energia: `get_power` e o evento `"power"` (estado + últimos 8, só na memória) alimentam a Tela e energia, o card do Início, a barra lateral (bolinha vermelha em "Tela e energia" quando o último envio falhou) O motivo separa as etapas: resolução (não achou o IP), conexão (o IP não conectou), esperando a resposta, chave errada, HTTP nnn. A mesma linha vai pro log em arquivo (`power: sleep (SystemSleep, /sleep) ok em 180 ms, 1 tentativa(s), IP 192.168.0.42 (cache), o sono esperou 180 ms`).
- macOS: **Rede Local**. Desde o macOS 15, app fora do Terminal precisa dessa permissão até pra resolver `cydex.local`; sem ela dá `No route to host` (erro 65) por IP e `nodename nor servname` no nome. O `src-tauri/Info.plist` declara `NSLocalNetworkUsageDescription` e `NSBonjourServices` (`_http._tcp`); sem isso o sistema bloqueia sem perguntar. O app não tem certificado: o `tauri.conf.json` assina ad hoc (`bundle.macOS.signingIdentity: "-"`), o que amarra o Info.plist e dá o Identifier `dev.cydex.desktop`. Sem isso o binário sai só *linker-signed* (Identifier `cydex_desktop`, Info.plist solto) e o macOS bloqueia a Rede Local sem perguntar (foi o 0.2.0; conserto manual: `codesign --force --deep -s - /Applications/CYDex.app`). Mesmo assim o macOS identifica pelo hash: **cada build novo pede a permissão de novo**. No app empacotado o mdns-sd não recebe nada; quem resolve é o `getaddrinfo` (e a varredura, se tudo falhar).
- A varredura abre no máximo 64 conexões por vez: app aberto pelo Finder tem limite de 256 arquivos, e 759 de uma vez falhavam na hora (inclusive a do CYD). A sonda para ao ler a assinatura, porque o CYD espera o cliente fechar primeiro.
- Log em arquivo (`tauri-plugin-log`; macOS: `~/Library/Logs/dev.cydex.desktop/CYDex.log`): cada requisição e a cadeia inteira do erro, por qual caminho o IP foi achado e cada envio de energia (linhas `power:`). Em Este app, "Abrir o log" e "Mostrar na pasta" (`open_log`, sem plugin: `open`/`explorer`/`xdg-open`).
- Dock e bandeja: com a janela aberta o app fica no Dock como um app normal (`ActivationPolicy::Regular`); fechar a janela esconde e tira do Dock (`Accessory`), e ele segue na bandeja. Clicar no ícone do Dock com a janela escondida abre de novo (`RunEvent::Reopen`). Sair (bandeja ou ⌘Q) fecha de vez. Tudo em `set_in_dock`/`show_window`; Windows e Linux não mudam (janela escondida já sai da barra de tarefas). Aberto pelo início com o sistema (`--hidden`) fica só na bandeja; sem chave, abre a janela na Conexão.
- Textos da janela em `src/i18n.ts` (pt-BR referência + en), DOM por `el()` (`src/dom.ts`, sem `innerHTML`: título de aviso vem da rede).
- Janela: barra lateral de 184 px (`index.html`, `main.ts`) e uma seção por arquivo em `src/views/` (`View { render, onStatus }`; peças comuns em `views/common.ts`: cabeçalho, interruptor, linha de ajuste). No pé da barra, o cartão de estado (tocar abre a Conexão). `minWidth` 720.

### Telas do app (página "App do computador" do canvas)

| # | Arquivo no canvas | Seção | Observações |
|---|---|---|---|
| — | `DeskNav.dc.html` | Barra lateral | Início, Saves, Avisos, Tela e energia, Ajustes do CYD; linha; Conexão, Este app. Pé: estado do CYD (conexão, versão, sinal) |
| D01 | `DeskInicio.dc.html` | Início (`views/home.ts`) | Anel + barras por geração; cards "Tela do CYD" e "Saves" |
| D01b | `DeskOffline.dc.html` | Início fora da rede | Faixa com "Procurar de novo"; progresso vazio |
| D02 | `DeskSaves.dc.html` | Saves (`views/saves.ts`) | Soltar/escolher, tabela, Recarregar. D02b `DeskSavesEnviando`: arrastando + relatório |
| D03 | `DeskAvisos.dc.html` | Avisos (`views/notices.ts`) | Abertos, indicadores da Vitrine (`GET /status`, api ≥ 4), aviso de teste recolhido |
| D04 | `DeskEnergia.dc.html` | Tela e energia (`views/power.ts`) | Interruptor, "Agora" (estado + rede de segurança), últimos envios com o motivo. D04b `DeskEnergiaAntigo`: api < 5 |
| D05 | `DeskConexao.dc.html` | Conexão (`views/connection.ts`) | Endereço, chave, "Como o app acha o CYD" (`get_locate`). D05b `DeskPrimeiraVez`: sem chave, passo a passo |
| D06 | `DeskAjustesTela.dc.html` | Ajustes do CYD (`views/settings.ts`) | Abas Tela e som (brilho de dia/noite, apagar tela, som + cidade do pôr do sol) e Vitrine e navegação (D06b). `get_cyd_settings`/`set_cyd_setting`: um campo por vez, slider grava ao soltar, o CYD devolve tudo. Na 1ª leitura, "Obtendo os ajustes do CYDex…" (o `GET` espera na fila do CYD atrás do status, dos saves e dos avisos); voltar à seção mostra os últimos lidos enquanto relê. D06c: firmware sem `/settings` (api < 6) = faixa + controles desativados, só a cidade funciona |
| D07 | `DeskApp.dc.html` | Este app (`views/app.ts`) | Abrir com o sistema, idioma da janela, log |
| D08 | `DeskBandeja.dc.html` | Menu da bandeja (`tray_text.rs`) | Estado (só leitura), Apagar/Acender agora, Fechar todos os avisos, Abrir, Sair. O interruptor de acompanhar o computador fica só na Tela e energia |
- Testes: `cargo test`; `cargo test -- --ignored live_device --nocapture` roda contra o CYD de verdade (chave do `~/.cydex`).
- O `tools/cydex_claude.sh` (hook do Claude Code) continua fora do app. O vigia em Swift + LaunchAgent que existia antes foi removido (o app faz o mesmo nos três sistemas).

## Futuro / em aberto

- Troca entre aparelhos via ESP-NOW.
- Baixar saves pelo app (backup): precisa antes resolver o link do Wi-Fi morrendo sob TX contínuo (ver "Saves pela API"). Um vigia de link (ping no gateway, reconectar) também protegeria os avisos.
- App: pasta sincronizada (vigiar a pasta de saves do emulador e enviar sozinho) e trocar o `tools/cydex_claude.sh` por um comando do app.
- **Sprites brilhantes.** Criatura com ★ (Time, Caixas, Membro, Hall da Fama, "Meu time" da Vitrine) mostra o sprite shiny. No SD, arquivos próprios no mesmo `SpriteFile.h` (`sprites_shiny_96/48/24.bin`, o `front_shiny` da PokeAPI no `tools/build_sd.py`); 64 não precisa (Evolução é da espécie). Sem arquivo (SD antigo), cai no normal. Memória igual: mesma vaga, só muda o arquivo lido.

Pendências pequenas (vistas nos prints):
- Evolução: nome longo sai cortado no meio ("Charmanc"); usar "…" (`ui_label_clamp`).
- Saves: os não reconhecidos vão pro fim da lista (hoje é a ordem do cartão).
- Caixas: segurar ‹ › não repete (o canvas pede `HoldRepeat`).
- Canvas: trocar "FireRed/LeafGreen" e "Ruby/Sapphire" por "FR/LG" e "R/S", como no firmware.
- README: refazer o `saves.png` (mostra "1 saves", de antes do singular).
