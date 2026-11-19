# CYDex

Um Dex de mesa para o **CYD** ("Cheap Yellow Display", placa ESP32-2432S028 de 2,8"). Ele mostra as 1025 espécies do catálogo nacional, marca quais você já viu ou capturou lendo os seus saves e tem um **modo vitrine** para ficar ligado na mesa, trocando de espécie sozinho.

Tudo roda na placa. Os dados das espécies e os sprites ficam no cartão SD e são gerados no PC por um script. O Wi-Fi é opcional e só serve para receber os **avisos do Claude Code**.

> Projeto de fã, sem relação com a Nintendo, a Game Freak ou a The Pokémon Company. O repositório não traz sprites nem dados dos jogos: o script baixa tudo da [PokeAPI](https://pokeapi.co/) na sua máquina.

## O que tem

- **Catálogo**: Lista (4 por página) e Grade (10 por página, com sprite), filtros Todos / Capturados / Faltando, filtro por até 2 tipos e busca por nome ou número. Também dá para pular direto para uma geração ou um número.
- **Ficha**: sprite, tipos, altura, peso, geração, status base e linha evolutiva com níveis. Espécies que você não viu aparecem como `???`.
- **Progresso**: % capturado e uma barra por geração (I–IX).
- **Modo vitrine**: uma espécie por vez, com sprite em 2×, trocando a cada 30 s a 15 min, em ordem sequencial ou aleatória. Pode mostrar só capturados ou todos os vistos e tem brilho próprio. Pode abrir direto ao ligar.
- **Ajustes**: brilho, som ao tocar, abrir em Lista ou Grade, apagar a tela depois de um tempo sem toque, idioma (português e inglês) e recarregar os saves.
- **Avisos do Claude Code**: com o Wi-Fi ligado, o CYD recebe os hooks do Claude Code e avisa quando ele precisa de permissão, está esperando você ou terminou. Aparece uma faixa no topo de qualquer tela, ou um balão na vitrine, com bipe.
- **Saves**: Gen3 (FireRed/LeafGreen, Ruby/Sapphire/Emerald). O resultado fica em cache no cartão e só é refeito quando um save muda.

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

### 3. Avisos do Claude Code (opcional)

1. No CYD, abra **Ajustes › Sistema › Wi-Fi**, ligue o Wi-Fi, escolha a rede e digite a senha. Ele volta a conectar sozinho quando liga.
2. Em **Wi-Fi › ⓘ** aparecem o endereço (`cydex.local`), o IP e a **chave do hook** (8 caracteres, gerada no primeiro boot).
3. No computador, exporte a chave (por exemplo no `~/.zshrc`):

   ```sh
   export CYDEX_KEY=a1b2c3d4
   ```

4. Copie os hooks de [`tools/claude-hooks.example.json`](tools/claude-hooks.example.json) para o `.claude/settings.json` do projeto, ou para `~/.claude/settings.json` se quiser os avisos em todos os projetos. Se já houver uma chave `hooks`, junte as listas.
5. Teste sem o Claude: `CYDEX_KEY=... tools/cydex_notify.sh permission` (ou `idle`, `stop`, `ping`). A resposta tem que ser `204`. Depois, `clear` fecha o aviso, como se você tivesse respondido no Claude.
6. Abra o `claude` de novo num terminal onde o `CYDEX_KEY` esteja exportado. O `/hooks` deve listar `Notification` e `Stop`.

Quando você responde no Claude (manda uma mensagem, aprova uma permissão ou fecha a sessão), o aviso daquela sessão some sozinho do CYD.

O hook passa o JSON por `jq` (que vem no macOS) pra mandar só o que o CYD usa, em menos de 1 KB, e envia com `curl -4` e `async: true`. Ele roda em segundo plano, nunca segura o Claude e, se o CYD estiver desligado, só falha sem dizer nada. O `-4` é necessário: o CYD só tem IPv4, e sem ele o macOS espera 5 s pela resposta IPv6 do `cydex.local`. Com o nome em vez do IP, o hook continua funcionando quando o roteador der outro IP ao CYD.

## Desenvolvimento

```sh
pio test -e native_test   # testes no PC (lógica, parser Gen3, idiomas, avisos)
```

A lógica que não depende da placa fica em `lib/core` (C++ puro) e tem testes nativos: parser de save, formato dos arquivos, paginação, busca, filtros, vitrine e textos.

### Estrutura

```
include/            pinos, setup do display, lv_conf.h, versão
lib/core/src/       C++ puro, testado no PC
  dex/              catálogo, status visto/capturado, formatos do SD
  saves/            leitura de saves (gen3/; outras gerações entram aqui)
  logic/            paginação, filtro, busca, vitrine, texto que cabe na fonte
  notify/           JSON do hook do Claude Code (em fluxo) -> aviso, fila
  net/              pedido HTTP em fluxo, lista de redes Wi-Fi
  i18n/             textos (um arquivo por idioma em lang/)
src/
  hal/              display, toque, SD, backlight, alto-falante
  data/             espécies, sprites, status, ajustes (NVS)
  net/              Wi-Fi (wifi_mgr) e servidor do hook (hook_server)
  ui/               tema, widgets, navegação, uma tela por arquivo em screens/
test/               test_core, test_gen3, test_i18n, test_notify
tools/              build_sd.py (SD), gen_fonts.mjs e gen_icons.mjs (assets),
                    claude-hooks.example.json e cydex_notify.sh (avisos)
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
- Troca entre aparelhos via ESP-NOW.

## Licença

O código do CYDex está sob a [licença MIT](LICENSE).

Ela não cobre o que vem de terceiros:

- **Fonte Montserrat**: SIL Open Font License 1.1. As fontes geradas em `src/ui/fonts/` derivam dela.
- **LVGL** (MIT) e **TFT_eSPI** (FreeBSD/MIT): baixados pelo PlatformIO.
- **Dados e sprites das espécies**: baixados da PokeAPI pelo `tools/build_sd.py` na sua máquina e nunca incluídos aqui. Os nomes, as imagens e as marcas pertencem aos seus donos.
