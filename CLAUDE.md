# CYDex

Pokédex de mesa rodando num **CYD (Cheap Yellow Display, ESP32-2432S028)** de 2,8". Mostra o catálogo de espécies (capturadas, vistas ou desconhecidas) lido do cartão SD, tem um **modo vitrine** sempre ligado e, mais pra frente, vai receber avisos de um servidor de webhooks.

Nome: **CYDex** (CYD + dex, lê-se "sai-dex"). Não usar "Poké"/"Pokémon" no nome do projeto, do repositório ou de binários; "Dex" sozinho é genérico.

## Layout (fonte da verdade da UI)

- Canvas com todas as telas: https://claude.ai/artifact/3cxMPNMPxtGhqaFMbF4uFy
- Cada tela é um artboard de **320×240** (tamanho real do display). No modo Play elas navegam entre si.
- Quando a UI mudar, atualizar o canvas primeiro e depois o firmware.

### Telas

| # | Arquivo no canvas | Tela | Observações |
|---|---|---|---|
| 01 | `Main.dc.html` | Boot | Logo + barra de progresso "Lendo cartão SD… 642 / 1025 registros" |
| 02 | `ErroSD.dc.html` | Sem cartão SD | Botão "Tentar de novo" (44 px). Pede FAT32 |
| 03 | `Menu.dc.html` | Menu | Barra de capturados (312/1025), tile grande Lista + Buscar, Tipos, Progresso, Ajustes; botão "Vitrine" no topo. Sem indicador de SD (sem cartão o aparelho não passa do Boot) |
| 04 | `Progresso.dc.html` | Progresso | Anel de % capturados + barras por geração (I–IX) |
| 05 | `Ajustes.dc.html` | Ajustes | Brilho, som ao tocar, abrir em Lista/Grade, apagar tela, Modo vitrine ›, Sistema › |
| 05b | `AjustesVitrine.dc.html` | Ajustes › Modo vitrine | Intervalo, ordem, fonte das espécies, brilho próprio, iniciar ao ligar; ▶ no cabeçalho inicia a vitrine |
| 05c | `AjustesSistema.dc.html` | Ajustes › Sistema | Idioma (uma linha de 34 px por idioma, nome escrito no próprio idioma, troca na hora) + Cartão SD (registros, saves, "Recarregar") |
| 06 | `Lista.dc.html` | Lista | Cabeçalho: buscar, tipos, ver em grade. Filtro Todos/Capturados/Faltando, 4 linhas por página, rodapé de paginação |
| 06b | `IrPara.dc.html` | Ir para | 9 gerações (3×3) + teclado numérico (#1–1025). Os dois levam à página que contém o número, na visão de origem (Lista ou Grade); não abrem a Ficha |
| 06c | `ListaFiltro.dc.html` | Lista com filtro | Com busca ou tipo ativo, o "Nacional" vira um chip (tocar limpa). Vale pra Grade também |
| 07 | `Grade.dc.html` | Grade | 5×2 (10 por página), sprite 48×48, rodapé de paginação |
| 08 | `Busca.dc.html` | Busca | Campo no cabeçalho, sugestões (até 3), teclado QWERTY de 40 px + modo 123. Ignora maiúsculas, acentos e pontuação; nome só casa com espécie vista; "Ver" vira filtro |
| 09 | `Tipos.dc.html` | Filtro por tipo | 18 tipos em grade 4×5, até 2 selecionados (o 3º troca o mais antigo), "Aplicar (n)" |
| 10 | `Detalhe.dc.html` | Ficha: Info | Sprite 96×96, nome, tipos, altura, peso, geração, status de captura |
| 11 | `DetalheStatus.dc.html` | Ficha: Status | 6 barras (PS, Atq, Def, AtE, DfE, Vel) + total |
| 12 | `DetalheEvol.dc.html` | Ficha: Evolução | Até 3 estágios 64×64 com nível entre eles |
| 13 | `NaoVisto.dc.html` | Ficha: não visto | Tudo "???", abas Status/Evolução desativadas |
| 14 | `Vitrine.dc.html` | Modo vitrine | Sprite 2×, nome, tipos, medidas, mini status; toque sai pro Menu |

## Hardware

- **ESP32-2432S028 (sem o "R")**, a mesma placa física do PokEsp (`~/Development/PokEsp`). Display 320×240 com controlador **ST7789** (a serigrafia sugere ILI9341, mas não é), usado em **paisagem** (`setRotation(1)`). Precisa de `TFT_RGB_ORDER TFT_BGR` + `TFT_INVERSION_OFF` (`include/tft_setup.h`). **Sem PSRAM.**
- Três barramentos separados, sem mutex de SPI: display na HSPI (12/13/14/15), toque XPT2046 resistivo por **bit-bang** próprio (25/32/39/33; o touch embutido do TFT_eSPI não serve) e microSD na VSPI (18/19/23/5). Pinos só em `include/board_pins.h`. Backlight por PWM (ledc) no GPIO 21. Alto-falante no conector P4 (GPIO 26).
- Lições do PokEsp (não repetir):
  - A pilha da loopTask fica em 64 KB (`getArduinoLoopTaskStackSize`). Struct grande em variável local na cadeia LVGL → parser → SD estoura. Medir com `-fstack-usage`, não estimar.
  - Sem bloco contíguo grande no heap. Ler arquivos grandes do SD em pedaços (`sd_read_at`).
  - Não abrir arquivo dentro do callback de `sd_list_files`. Listar primeiro, abrir depois.
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
- Ajustes (05, 05b, 05c): os valores ficam em `AppSettings` (`data/Settings`, NVS). As peças comuns (slider, interruptor, segmentado, "− valor +", linha "›") estão em `ui/settings_widgets.*`. Os sliders aplicam na hora e só gravam ao soltar.
- `ui/power.*` cuida do brilho, do "apagar tela" e do som ao tocar. As telas pedem brilho por `power_set_level()`, nunca direto no backlight. O 1º toque com a tela apagada só acende. Na Vitrine a tela nunca apaga. O som é um clique de 12 ms no alto-falante (GPIO 26, canal 2 do ledc, porque os canais 0 e 1 dividem o timer do backlight).
- A versão ("CYDex v0.1") fica em `include/version.h`.
- A lógica sem LVGL fica em `lib/core/src/logic/` (`Pager`, `HoldRepeat`, `Catalog`, `NumberInput`, `Format`, `Search`, `Showcase`) e tem testes nativos.
- O LVGL 8 não tem `margin`. Usar um invólucro com padding.
- Sem tema padrão do LVGL (`LV_USE_THEME_DEFAULT 0`): todo widget é estilizado à mão a partir dos tokens.

### Sprites

- Vêm do SD. Base 96×96.
- Ficha: 96×96 (1×). Evolução: 64×64. Grade: 48×48 (metade). Vitrine: 192×192 (2×, vizinho mais próximo).
- Usar só escalas inteiras ou metade; escala fracionária borra pixel art e custa mais.

## Modo vitrine

- Sempre ligado, mostra uma espécie por vez.
- Ajustes (salvos na NVS com `Preferences`, lidos ao entrar):
  - troca a cada: 30 s, 1, 2, 5 ou 15 min (padrão 1 min)
  - ordem: sequencial ou aleatória
  - mostrar: só capturados ou todos vistos
  - brilho próprio da vitrine (padrão 35%)
  - iniciar ao ligar (padrão desligado): o Boot vai direto pra Vitrine em vez do Menu
- Redesenha a tela inteira só na troca; entre trocas nada muda.
- Desloca o layout 1–2 px a cada troca (evitar retenção de imagem no LCD).
- Mini status: 6 barras sem rótulo na ordem PS, Atq, Def, AtE, DfE, Vel, escala até 150, valores ≥ 60 em accent, "Total N" em cima.

## Dados

- Catálogo nacional: 1025 espécies, 9 gerações (I 1–151, II 152–251, III 252–386, IV 387–493, V 494–649, VI 650–721, VII 722–809, VIII 810–905, IX 906–1025).
- Tudo vem do cartão SD (FAT32). Espécies e sprites são gerados no PC por `tools/build_sd.py` (PokeAPI). Status visto/capturado vem dos saves do usuário.

```
/cydex/species.bin          cabeçalho de 16 B ("CYDX", versão, count) + 1025 registros fixos de 64 B (seek em 16+(n-1)*64)
/cydex/sprites_96.bin       todos os 96×96 num arquivo: cabeçalho "CYSP" + registros fixos (lv_img_header + RGB565, chroma key 0x00FF00)
/cydex/sprites_64.bin       64×64 já reduzido no PC (Evolução)
/cydex/sprites_48.bin       48×48 já reduzido no PC (Grade)
/cydex/status.cache         gerado pelo CYD: assinatura dos saves + bits visto/capturado
/saves/*.sav, *.srm         saves (hoje só Gen3; uma geração nova é um parser novo em lib/core/saves)
```

- Os layouts ficam em `lib/core/src/dex/SpeciesRecord.h` e `SpriteFile.h`, que são a fonte da verdade e devem bater com o `tools/build_sd.py`.
- **Nunca uma pasta com milhares de arquivos.** No FAT32 cada abertura varre a pasta inteira: com 1025 sprites soltos, cada um levava ~560 ms. Use arquivo único com registros fixos e `seek`. O `sd_open_read` reaproveita o arquivo se o caminho for o mesmo. SPI do SD a 20 MHz (o padrão do `SD.begin` é 4 MHz).
- Os saves são lidos **só no boot** (ou em "Cartão SD › recarregar"). O resultado (`DexStatus`, 2×129 B) fica em RAM. Se a assinatura dos saves (nome, tamanho, data) não mudou, o boot usa o `status.cache` e pula o parse.
- Na RAM ficam só os nomes (para Lista e Busca). O registro completo é lido do SD quando a ficha abre.

## Futuro / em aberto

- Servidor que recebe webhooks do Claude e mostra os avisos no CYD (ele fica na mesa). Ideia de UI: a criatura da vitrine "entrega" o aviso num balão.
- Troca entre aparelhos via ESP-NOW.
- Definir: canal entre servidor de webhooks e CYD (Wi-Fi + HTTP/MQTT/WebSocket).
