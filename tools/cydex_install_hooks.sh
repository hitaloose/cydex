#!/bin/sh
# Junta os hooks do CYDex (claude-hooks.example.json, na mesma pasta deste
# script) num settings.json do Claude Code, sem mexer nos hooks que já estão lá.
#
#   tools/cydex_install_hooks.sh ~/.claude/settings.json            instala/atualiza
#   tools/cydex_install_hooks.sh --dry-run ~/.claude/settings.json  só mostra o resultado
#   tools/cydex_install_hooks.sh --remove ~/.claude/settings.json   tira os do CYDex
#
# - Pode rodar de novo à vontade: antes de juntar, tira os hooks do CYDex que já
#   estavam (os que chamam cydex_claude.sh ou o antigo http://cydex.local/hook).
# - O caminho do cydex_claude.sh no exemplo é trocado pelo caminho absoluto do
#   que está ao lado deste script (o repositório pode estar em qualquer lugar).
# - Guarda uma cópia em <settings>.bak-<data> antes de gravar. Arquivo que não
#   existe é criado com só os hooks.
# Precisa de jq (vem no macOS).
set -eu

usage() {
  echo "uso: $0 [--dry-run | --remove] <settings.json>" >&2
  exit 2
}

MODE=install
DRY=0
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY=1 ;;
    --remove) MODE=remove ;;
    -h | --help) usage ;;
    -*) usage ;;
    *) break ;;
  esac
  shift
done
[ $# -eq 1 ] || usage
SETTINGS="$1"

DIR="$(cd "$(dirname "$0")" && pwd)"
EXAMPLE="$DIR/claude-hooks.example.json"
CLAUDE_SH="$DIR/cydex_claude.sh"
command -v jq >/dev/null || { echo "precisa do jq" >&2; exit 1; }
[ -f "$EXAMPLE" ] || { echo "não achei $EXAMPLE" >&2; exit 1; }
[ -x "$CLAUDE_SH" ] || { echo "não achei $CLAUDE_SH (ou não é executável)" >&2; exit 1; }

if [ -f "$SETTINGS" ]; then
  jq empty "$SETTINGS" 2>/dev/null || { echo "$SETTINGS não é JSON válido; nada foi mudado" >&2; exit 1; }
  CURRENT="$(cat "$SETTINGS")"
else
  CURRENT='{}'
fi

# Comando com aspas: o caminho pode ter espaço.
CMD="\"$CLAUDE_SH\""

RESULT="$(printf '%s' "$CURRENT" | jq --slurpfile ex "$EXAMPLE" --arg cmd "$CMD" --arg mode "$MODE" '
  def is_cydex: (.command // "") | test("cydex_claude\\.sh|cydex\\.local/hook");
  # Tira os hooks do CYDex de cada grupo; grupo e evento que ficam vazios saem.
  def strip:
    if .hooks then
      .hooks |= (with_entries(.value |= (map(.hooks |= map(select(is_cydex | not)))
                                        | map(select((.hooks | length) > 0))))
                 | with_entries(select((.value | length) > 0)))
    else . end;
  ($ex[0].hooks
    | with_entries(.value |= map(.hooks |= map(if is_cydex then .command = $cmd else . end))))
    as $mine
  | strip
  | if $mode == "install" then
      reduce ($mine | to_entries[]) as $e (.; .hooks[$e.key] = ((.hooks[$e.key] // []) + $e.value))
    else . end
  | if .hooks == {} then del(.hooks) else . end
')"

if [ "$DRY" -eq 1 ]; then
  printf '%s\n' "$RESULT" | jq .
  exit 0
fi

if [ "$RESULT" = "$(printf '%s' "$CURRENT" | jq .)" ]; then
  echo "nada mudou em $SETTINGS"
  exit 0
fi

if [ -f "$SETTINGS" ]; then
  STAMP="$(date +%Y%m%d-%H%M%S)"
  BACKUP="$SETTINGS.bak-$STAMP"
  # Duas gravações no mesmo segundo não podem apagar a cópia anterior.
  n=1
  while [ -e "$BACKUP" ]; do
    n=$((n + 1))
    BACKUP="$SETTINGS.bak-$STAMP.$n"
  done
  cp -p "$SETTINGS" "$BACKUP"
  echo "cópia em $BACKUP"
else
  mkdir -p "$(dirname "$SETTINGS")"
fi
# Escreve por cima (não mv): mantém dono, permissões e links do arquivo.
printf '%s\n' "$RESULT" > "$SETTINGS"

if [ "$MODE" = install ]; then
  echo "hooks do CYDex instalados em $SETTINGS ($(printf '%s' "$RESULT" | jq '[.hooks[][] .hooks[] | select(.command == $c)] | length' --arg c "$CMD") entradas)"
  [ -f "$HOME/.cydex" ] || echo "falta ~/.cydex com CYDEX_KEY=<chave> (a chave aparece no CYD em Ajustes › Sistema › Wi-Fi › ⓘ)"
else
  echo "hooks do CYDex removidos de $SETTINGS"
fi
