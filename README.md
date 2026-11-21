# CYDex

Um Dex de mesa para o **CYD** ("Cheap Yellow Display", placa ESP32-2432S028 de 2,8"). Ele mostra as 1025 espécies do catálogo nacional, marca quais você já viu ou capturou lendo os seus saves e tem um **modo vitrine** para ficar ligado na mesa, trocando de espécie sozinho.

Tudo roda na placa. Os dados das espécies e os sprites ficam no cartão SD e são gerados no PC por um script. O Wi-Fi é opcional e serve para receber **avisos** pela rede: qualquer programa ou script manda um aviso com um `curl`, e o Claude Code já vem integrado.

> Projeto de fã, sem relação com a Nintendo, a Game Freak ou a The Pokémon Company. O repositório não traz sprites nem dados dos jogos: o script baixa tudo da [PokeAPI](https://pokeapi.co/) na sua máquina.

## O que tem

- **Catálogo**: Lista (4 por página) e Grade (10 por página, com sprite), filtros Todos / Capturados / Faltando, filtro por até 2 tipos e busca por nome ou número. Também dá para pular direto para uma geração ou um número.
- **Ficha**: sprite, tipos, altura, peso, geração, status base e linha evolutiva com níveis. Espécies que você não viu aparecem como `???`.
- **Progresso**: % capturado e uma barra por geração (I–IX).
- **Modo vitrine**: uma espécie por vez, com sprite em 2×, trocando a cada 30 s a 15 min, em ordem sequencial ou aleatória. Pode mostrar só capturados ou todos os vistos e tem brilho próprio. Pode abrir direto ao ligar.
- **Ajustes**: brilho, som ao tocar, abrir em Lista ou Grade, apagar a tela depois de um tempo sem toque, idioma (português e inglês) e recarregar os saves.
- **Avisos**: com o Wi-Fi ligado, o CYD tem uma API HTTP simples pra mostrar avisos com título, texto, cor, ícone e prazo. Aparece uma faixa no topo de qualquer tela, ou um balão na vitrine, com bipe. Integrações prontas: **Claude Code** (avisa quando ele precisa de permissão, está esperando você ou terminou).
- **Saves**: Gen3 (FireRed/LeafGreen, Ruby/Sapphire/Emerald). O resultado fica em cache no cartão e só é refeito quando um save muda.
- **App no computador** (macOS, Windows e Linux): fica na bandeja, apaga e acende a tela do CYD junto com o computador, envia e apaga saves pelo Wi-Fi, mostra o progresso da Dex e os avisos abertos.

A interface não tem scroll, animação nem tema claro, de propósito: cada toque faz um redesenho só, e o painel TN do CYD fica ruim com fundo claro fora do ângulo.

## Hardware

- **ESP32-2432S028** (a versão sem "R"). O controlador do display é **ST7789**, apesar de a serigrafia dizer ILI9341. A placa não tem PSRAM.
- Cartão **microSD em FAT32**.
- Opcional: alto-falante no conector **P4 ("SPEAK")** para o som ao tocar.

Os pinos estão em [`include/board_pins.h`](include/board_pins.h) e a configuração do display em [`include/tft_setup.h`](include/tft_setup.h). A versão "R" ou outras variantes do CYD podem precisar de outro driver ou de outra ordem de cores.

## Como usar

### 1. Gerar o cartão SD

Requer Python 3.9+ e Pillow (`pip install pillow`).

```sh
python3 tools/build_sd.py --out build/sd
```

A primeira vez baixa as 1025 espécies e leva alguns minutos. As respostas da PokeAPI ficam em `tools/.cache`, então rodar de novo é rápido. A saída tem uns 32 MB:

```
build/sd/
  cydex/species.bin       dados das espécies
  cydex/sprites_96.bin    sprites 96×96 (Ficha e Vitrine)
  cydex/sprites_64.bin    sprites 64×64 (Evolução)
  cydex/sprites_48.bin    sprites 48×48 (Grade)
  saves/                  coloque seus saves aqui
```

Copie o conteúdo de `build/sd/` para a raiz do cartão. Coloque os saves em `/saves` com extensão `.sav` ou `.srm` (os de emulador servem). O CYDex junta todos: uma espécie conta como vista ou capturada se aparecer assim em qualquer save.

Os nomes das espécies saem em inglês, que no Brasil são os nomes oficiais. Com `--names-lang fr` (ou `es`, `de`, `it`) eles saem em outro idioma.

### 2. Gravar o firmware

Requer [PlatformIO](https://platformio.org/).

```sh
pio run -e cyd -t upload
pio device monitor        # log serial a 115200
```

O upload fica em 115200 baud, porque o CH340 desta placa dá erro a 921600. Se a porta não for detectada sozinha, passe `--upload-port /dev/cu.usbserial-XXXX`.

Na primeira vez que liga, o CYDex lê o cartão (~0,3 s com o cache), monta o status a partir dos saves e abre o Menu.

### 3. Avisos (opcional)

O CYD serve uma API em `http://cydex.local`, sem nada rodando no computador. Qualquer coisa que faça um pedido HTTP pode mandar aviso: um script, um build, um cron, o Home Assistant, outro programa. Quem manda decide o título, o texto, a cor, o ícone e por quanto tempo o aviso fica.

#### Ligar o Wi-Fi e pegar a chave

1. No CYD, abra **Ajustes › Sistema › Wi-Fi**, ligue o Wi-Fi, escolha a rede e digite a senha. Ele volta a conectar sozinho quando liga.
2. Em **Wi-Fi › ⓘ** aparecem o endereço (`cydex.local`), o IP e a **chave da API** (8 caracteres, gerada no primeiro boot).
3. No computador, grave a chave num arquivo que só você lê. Os scripts de `tools/` leem dele (uma variável `CYDEX_KEY` já exportada vale mais):

   ```sh
   printf 'CYDEX_KEY=a1b2c3d4\n' > ~/.cydex && chmod 600 ~/.cydex
   ```

4. Teste: `tools/cydex_notify.sh ping` mostra a versão; `tools/cydex_notify.sh error` mostra um aviso de erro; `tools/cydex_notify.sh clear` fecha todos.

#### Mandar um aviso

```sh
. ~/.cydex   # carrega o CYDEX_KEY
curl -4 -H "Authorization: Bearer $CYDEX_KEY" \
  --data-binary '{"id":"build:cydex","type":"error","title":"Build falhou","subtitle":"cydex · main","message":"test_i18n: falta um texto em en.c","timeout":600}' \
  http://cydex.local/notices
# {"id":"build:cydex"}
```

Sempre use `curl -4` (ou o IP): o CYD só tem IPv4, e sem o `-4` o macOS espera 5 s pela resposta IPv6 do `cydex.local`. Com o nome em vez do IP, o cliente continua funcionando quando o roteador der outro IP ao CYD.

| Campo | Tipo | Para que serve |
|---|---|---|
| `title` | texto, **obrigatório** | Linha de cima, em destaque (~28 caracteres cabem) |
| `subtitle` | texto | De onde vem o aviso (um projeto, um servidor). Na faixa aparece como "subtítulo · mensagem"; no balão da vitrine, em destaque |
| `message` | texto | Detalhe, até 2 linhas na faixa. A vitrine não mostra |
| `id` | até 40 de `A-Z a-z 0-9 . _ : -` | Identidade do aviso. Mandar de novo com o mesmo `id` **substitui** o aviso aberto (é o estado atual de uma coisa: um build, um chat). Sem `id`, o CYD cria um e devolve na resposta |
| `tag` | até 16, mesmo alfabeto | Rótulo pra fechar só aquele tipo de aviso (`DELETE …?tag=`) |
| `type` | `info`, `success`, `warning`, `waiting`, `question`, `error` | Preset de cor, ícone e prioridade (padrão `info`) |
| `priority` | `high`, `normal`, `low` | Sobrescreve a do `type`. A faixa mostra o mais importante |
| `color` | `#RRGGBB` | Sobrescreve a cor do `type` (barra, ícone, borda do balão) |
| `icon` | `info`, `check`, `alert`, `shield`, `clock`, `bell` | Sobrescreve o ícone do `type`. Um nome que o firmware não conhece usa o do `type` |
| `timeout` | segundos, 0–86400 | Fecha sozinho depois disso. `0` (padrão) fica até tocar ou fechar pela API |
| `sound` | `true`/`false` | Toca o bipe (padrão `true`; respeita o "Som ao tocar") |
| `wake` | `true`/`false` | Acende a tela (padrão `true`). Com `sound` e `wake` em `false`, o aviso é silencioso |

| `type` | Cor | Ícone | Prioridade |
|---|---|---|---|
| `question` | vermelho `#FF5A4E` | escudo | alta |
| `error` | vermelho `#FF5A4E` | alerta | alta |
| `warning` | laranja `#FF8A4C` | alerta | normal |
| `waiting` | amarelo `#F5D547` | relógio | normal |
| `success` | verde `#6FCF6A` | check | baixa |
| `info` | cinza `#93A0AD` | info | baixa |

Os textos passam por um filtro que tira markdown, junta espaços e troca o que a fonte não tem (emoji some). Campos que o CYD não conhece são ignorados.

#### Rotas

Todas pedem `Authorization: Bearer <chave>`, menos `GET /`.

| Rota | O que faz | Resposta |
|---|---|---|
| `POST /notices` | Cria ou substitui um aviso | `201 {"id":"…"}`; `400 {"error":"<campo>"}` com valor inválido (ou `"json"`); `413` com corpo > 4 KB |
| `DELETE /notices/<id>` | Fecha o aviso. Com `?tag=x`, só se a tag bater | `204` (também se já não estava aberto) |
| `DELETE /notices` | Fecha todos | `204` |
| `GET /notices` | Lista os abertos (`id`, `tag`, `title`, `type`, `age` em segundos) | `200` |
| `POST /sleep`, `POST /wake` | Apaga e acende a tela (ver o passo 4) | `204` |
| `GET /saves` | Lista os saves do cartão (`name`, `size`, `mtime`, `format`; `format` é `null` se o CYD não reconhece o save) | `200` |
| `PUT /saves/<nome>` | Grava o save (corpo = o arquivo, até 1 MB). O nome vai em URL (`%20`) e termina em `.sav` ou `.srm` | `201`; `400 {"error":"name"}`; `413`; `507` sem espaço |
| `DELETE /saves/<nome>` | Apaga o save | `204` |
| `POST /saves/reload` | Relê os saves (como Ajustes › Sistema › Recarregar) | `200 {"saves","parsed","seen","caught"}` |
| `GET /dex` | Bits visto/capturado em hex (bit = número − 1) | `200` |
| `GET /` | Nome, versão, `api` (2 = tem `/saves` e `/dex`) e sinal do Wi-Fi | `200` |

Chave errada dá `401`. Fechar um aviso não acende a tela nem toca nada. Enviar um save não relê nada sozinho: mande um `POST /saves/reload` depois do lote.

```sh
curl -4 -X PUT -H "Authorization: Bearer $CYDEX_KEY" --data-binary @emerald.sav http://cydex.local/saves/emerald.sav
curl -4 -X POST -H "Authorization: Bearer $CYDEX_KEY" http://cydex.local/saves/reload
```

#### Como os avisos aparecem

- Ficam abertos até **4** ao mesmo tempo, ordenados por prioridade (no empate, o mais antigo antes). Fora da vitrine, a faixa mostra o primeiro e um "+n" com os outros; na vitrine aparecem até 3 balões.
- Com 4 abertos, um aviso novo só entra se for mais importante que o último, que sai. Senão a resposta é `201` mas ele não aparece.
- O mesmo `id` com o mesmo título em menos de 3 s conta uma vez só (sem bipe repetido).
- Tocar na faixa ou num balão fecha aquele aviso.

#### Integrações

Integrações prontas, em `tools/`. Cada uma é um script que traduz os eventos de um programa em chamadas da API, então o firmware não precisa mudar quando uma integração nova entra.

##### Claude Code

Avisa quando o Claude **precisa de permissão**, está **esperando você** ou **terminou**, um aviso por chat, com o nome da pasta do projeto. Quando você responde no Claude (manda uma mensagem, aprova a permissão ou fecha a sessão), o aviso daquele chat some sozinho. Responder num chat não apaga o aviso de outro.

1. Faça o passo "Ligar o Wi-Fi e pegar a chave" acima (o script lê a chave do `~/.cydex`).
2. Instale os hooks no settings do Claude Code. O instalador junta os hooks do CYDex com os que você já tem (de outras ferramentas, por exemplo) sem mexer neles, guarda uma cópia do arquivo antes e pode rodar de novo quando quiser:

   ```sh
   tools/cydex_install_hooks.sh ~/.claude/settings.json     # todos os projetos
   tools/cydex_install_hooks.sh .claude/settings.json       # só um projeto
   tools/cydex_install_hooks.sh --dry-run ~/.claude/settings.json   # só mostra como fica
   tools/cydex_install_hooks.sh --remove ~/.claude/settings.json    # tira os do CYDex
   ```

3. Abra o `claude` de novo. O `/hooks` deve listar o CYDex em `Notification`, `Stop`, `UserPromptSubmit`, `PostToolBatch` e `SessionEnd`.

Os hooks estão em [`tools/claude-hooks.example.json`](tools/claude-hooks.example.json) e chamam [`tools/cydex_claude.sh`](tools/cydex_claude.sh), que lê o evento com `jq` (vem no macOS) e chama a API:

| Evento do Claude Code | Aviso |
|---|---|
| Pediu permissão | `question` "Claude precisa de permissão" |
| Esperando você | `waiting` "Claude está esperando você" |
| Terminou de responder | `success` "Claude terminou", com o começo da resposta |
| Você mandou mensagem ou fechou a sessão | fecha o aviso do chat |
| Você aprovou a permissão | fecha só o "precisa de permissão" |

O `id` do aviso é o `session_id` do chat. Os títulos saem em português; com `CYDEX_LANG=en` no `~/.cydex`, em inglês. Os hooks rodam em segundo plano (`async`), nunca seguram o Claude e, se o CYD estiver desligado, só falham sem dizer nada. O texto que você digita nunca sai do computador.

##### Nova integração

Crie um script em `tools/` que chame a API (veja o `cydex_claude.sh` como exemplo), escolha um `id` estável pra cada coisa que você acompanha (assim o aviso novo substitui o velho) e use `DELETE /notices/<id>` quando o assunto se resolver. Depois acrescente uma seção aqui em "Integrações".

### 4. App no computador (opcional)

O [`desktop/`](desktop/) é um app de bandeja para macOS, Windows e Linux (Tauri 2). Ele:

- apaga a tela do CYD quando a tela do computador apaga (ou ele dorme) e acende quando ele acorda. Vale até na Vitrine, e um toque acende como sempre;
- envia saves (arraste os `.sav`/`.srm` pra janela), apaga e relê o status;
- mostra o progresso da Dex por geração e os avisos abertos, e manda aviso de teste;
- acha o CYD na rede sozinho. A chave é a de Wi-Fi › ⓘ (ou a do `~/.cydex`, que ele importa na primeira vez).

No macOS, na primeira vez (e a cada versão nova), o sistema pergunta se o CYDex pode acessar a **rede local**: permita, senão ele não acha o CYD. Dá pra mudar depois em Ajustes › Privacidade e Segurança › Rede Local. Se algo não conectar, o log fica em `~/Library/Logs/dev.cydex.desktop/CYDex.log`.

Os instaladores ficam nas [releases](../../releases) (`.dmg` pro macOS, `.msi`/`.exe` pro Windows, `.AppImage`/`.deb`/`.rpm` pro Linux). Eles não são assinados: no macOS, se aparecer "está danificado", rode `xattr -cr /Applications/CYDex.app`; no Windows, no SmartScreen, "Mais informações" › "Executar assim mesmo".

Pra publicar uma versão nova (o workflow [`desktop-release.yml`](.github/workflows/desktop-release.yml) põe a versão da tag no app, compila nos três sistemas e publica a release quando todos terminam):

```sh
git tag desktop-v0.2.0 && git push origin desktop-v0.2.0
```

Também dá pra rodar pela aba Actions ("desktop-release" › "Run workflow", com a versão). O [`desktop.yml`](.github/workflows/desktop.yml) só roda testes e clippy nos três sistemas a cada push em `desktop/`.

Pra rodar do código:

```sh
cd desktop && npm install
npm run tauri dev     # janela de desenvolvimento
npm run tauri build   # instalador do sistema atual
```

Precisa de Rust 1.90+ e Node 20+. No Linux, também de `libwebkit2gtk-4.1-dev`, `libayatana-appindicator3-dev` e `librsvg2-dev`. Ainda não dá pra baixar saves do CYD pelo app (backup): mandar centenas de KB seguidos pelo Wi-Fi derrubava o link do CYD.

Com um aviso aberto, a tela não apaga; um aviso novo acende o CYD e, quando o último fecha, ele apaga de novo. Pra testar sem o app: `tools/cydex_notify.sh sleep` (ou `wake`).

## Desenvolvimento

```sh
pio test -e native_test   # testes no PC (lógica, parser Gen3, idiomas, avisos)
cd desktop/src-tauri && cargo test   # app do computador
```

A lógica que não depende da placa fica em `lib/core` (C++ puro) e tem testes nativos: parser de save, formato dos arquivos, paginação, busca, filtros, vitrine e textos.

### Estrutura

```
include/            pinos, setup do display, lv_conf.h, versão
lib/core/src/       C++ puro, testado no PC
  dex/              catálogo, status visto/capturado, formatos do SD
  saves/            leitura de saves (gen3/; outras gerações entram aqui)
  logic/            paginação, filtro, busca, vitrine, texto que cabe na fonte
  notify/           JSON do POST /notices (em fluxo) -> aviso, quadro de avisos
  net/              pedido HTTP em fluxo, lista de redes Wi-Fi
  i18n/             textos (um arquivo por idioma em lang/)
src/
  hal/              display, toque, SD, backlight, alto-falante
  data/             espécies, sprites, status, ajustes (NVS)
  net/              Wi-Fi (wifi_mgr) e servidor da API de avisos (notice_server)
  ui/               tema, widgets, navegação, uma tela por arquivo em screens/
test/               test_core, test_gen3, test_i18n, test_notify
desktop/            app de bandeja (Tauri 2): src/ é a janela (TypeScript),
                    src-tauri/ o Rust (cliente da API, mDNS, dormir/acordar
                    por sistema em power/)
tools/              build_sd.py (SD), gen_fonts.mjs e gen_icons.mjs (assets),
                    cydex_notify.sh (testa a API de avisos),
                    cydex_claude.sh, claude-hooks.example.json e
                    cydex_install_hooks.sh (integração com o Claude Code)
```

### Fontes e ícones

As fontes Montserrat e os ícones da interface são gerados para o formato do LVGL e já estão no repositório (`src/ui/fonts`, `src/ui/icons`). Para gerar de novo:

```sh
cd tools && npm install
npm run icons
npm run fonts   # precisa dos TTF Montserrat Medium/SemiBold/Bold em tools/fonts/
```

Os TTF não vão para o repositório. Eles vêm de [JulietaUla/Montserrat](https://github.com/JulietaUla/Montserrat) (licença OFL).

### Novo idioma

Copie `lib/core/src/i18n/lang/pt_br.c`, traduza e registre o idioma em `i18n.h` e `i18n.c`. O `test_i18n` falha se faltar algum texto, se os `%` não baterem ou se aparecer um caractere que a fonte não tem. As fontes cobrem Latin-1, então espanhol, francês, alemão e italiano cabem.

### Regras do projeto

Medidas, cores, regras de interface, formato dos dados e lições da placa (pilha, heap, SD) estão no [`CLAUDE.md`](CLAUDE.md).

## Próximos passos

- Saves de outras gerações (Gen1/2, Gen4+).
- Baixar saves do CYD pelo app (backup).
- Mais integrações de avisos.
- Troca entre aparelhos via ESP-NOW.

## Licença

O código do CYDex está sob a [licença MIT](LICENSE).

Ela não cobre o que vem de terceiros:

- **Fonte Montserrat**: SIL Open Font License 1.1. As fontes geradas em `src/ui/fonts/` derivam dela.
- **LVGL** (MIT) e **TFT_eSPI** (FreeBSD/MIT): baixados pelo PlatformIO.
- **Dados e sprites das espécies**: baixados da PokeAPI pelo `tools/build_sd.py` na sua máquina e nunca incluídos aqui. Os nomes, as imagens e as marcas pertencem aos seus donos.
