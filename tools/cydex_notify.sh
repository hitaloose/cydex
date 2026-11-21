#!/bin/sh
# Testa a API de avisos do CYD (POST/DELETE/GET /notices).
#
#   tools/cydex_notify.sh info|success|warning|waiting|question|error [título] [mensagem]
#   tools/cydex_notify.sh timeout     aviso que some sozinho em 10 s
#   tools/cydex_notify.sh silent      aviso sem som e sem acender a tela
#   tools/cydex_notify.sh bad         campo inválido (espera 400 {"error":"color"})
#   tools/cydex_notify.sh big         corpo de 60 KB (espera 413)
#   tools/cydex_notify.sh close [tag] fecha o aviso $CYDEX_ID (com tag, só se bater)
#   tools/cydex_notify.sh clear       fecha todos
#   tools/cydex_notify.sh list        lista os abertos
#   tools/cydex_notify.sh ping|sleep|wake
#
# CYDEX_ID muda o id (padrão "teste": o mesmo id substitui o aviso aberto).
# Chave e endereço: CYDEX_KEY e CYDEX_HOST (padrão cydex.local), do ambiente
# ou de ~/.cydex. A chave aparece no CYD em Ajustes › Sistema › Wi-Fi › ⓘ.
# O -4 é de propósito: o CYD só tem IPv4 e o macOS espera 5 s pela resposta
# IPv6 do mDNS sem ele. Precisa de jq.
set -eu

[ -f "$HOME/.cydex" ] && . "$HOME/.cydex"
HOST="${CYDEX_HOST:-cydex.local}"
KIND="${1:-info}"
ID="${CYDEX_ID:-teste}"
PROJECT="$(basename "$(pwd)")"

call() {
  curl -4 -sS -m 3 -w ' %{http_code}\n' -H "Authorization: Bearer ${CYDEX_KEY:-}" "$@"
}

post() {
  jq -nc "$@" | call -H 'Content-Type: application/json' --data-binary @- "http://$HOST/notices"
}

case "$KIND" in
  info | success | warning | waiting | question | error)
    post --arg id "$ID" --arg type "$KIND" --arg sub "$PROJECT" \
      --arg title "${2:-Aviso de teste ($KIND)}" \
      --arg msg "${3:-Mandado por tools/cydex_notify.sh, com **markdown** e acentuação.}" \
      '{id: $id, type: $type, title: $title, subtitle: $sub, message: $msg}'
    ;;
  timeout)
    post --arg id "$ID" --arg sub "$PROJECT" \
      '{id: $id, type: "info", title: "Some em 10 s", subtitle: $sub, timeout: 10}'
    ;;
  silent)
    post --arg id "$ID" --arg sub "$PROJECT" \
      '{id: $id, type: "info", icon: "bell", title: "Aviso silencioso", subtitle: $sub, sound: false, wake: false}'
    ;;
  bad)
    post '{title: "Cor errada", color: "vermelho"}'
    ;;
  big)
    FILLER="$(head -c 60000 /dev/zero | tr '\0' 'x')"
    post --arg msg "$FILLER" '{title: "Grande", message: $msg}'
    ;;
  close)
    if [ -n "${2:-}" ]; then call -X DELETE "http://$HOST/notices/$ID?tag=$2"
    else call -X DELETE "http://$HOST/notices/$ID"; fi
    ;;
  clear) call -X DELETE "http://$HOST/notices" ;;
  list) call "http://$HOST/notices" ;;
  sleep | wake) call -X POST "http://$HOST/$KIND" ;;
  ping) curl -4 -sS -m 3 "http://$HOST/"; echo ;;
  *)
    echo "uso: $0 info|success|warning|waiting|question|error|timeout|silent|bad|big|close|clear|list|ping|sleep|wake" >&2
    exit 2
    ;;
esac
