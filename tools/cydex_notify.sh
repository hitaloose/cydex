#!/bin/sh
# Manda um aviso de teste pro CYD, igual ao que o hook do Claude Code manda.
#
#   CYDEX_KEY=a1b2c3d4 tools/cydex_notify.sh permission|idle|stop|clear|approve|big|ping|sleep|wake
#
# clear = você respondeu (UserPromptSubmit): fecha os avisos do chat ($CYDEX_SESSION, padrão "teste").
# approve = aprovou a permissão (PostToolBatch): fecha só o "precisa de permissão".
#
# CYDEX_HOST muda o endereço (padrão cydex.local). O -4 é de propósito: o CYD
# só tem IPv4 e o macOS espera 5 s pela resposta IPv6 do mDNS sem ele. A chave aparece no CYD em Ajustes › Sistema › Wi-Fi › ⓘ.
set -eu

HOST="${CYDEX_HOST:-cydex.local}"
KIND="${1:-permission}"
# Chat simulado (um aviso por chat). Ex.: CYDEX_SESSION=b tools/cydex_notify.sh idle
SESSION="${CYDEX_SESSION:-teste}"
CWD="$(pwd)"

case "$KIND" in
  sleep|wake)
    # Apaga/acende a tela, como o Mac faz ao dormir/acordar (tools/cydex_power.sh).
    curl -4 -sS -m 3 -o /dev/null -w '%{http_code}\n' -X POST \
      -H "Authorization: Bearer ${CYDEX_KEY:-}" "http://$HOST/$KIND"
    exit 0
    ;;
  ping)
    curl -4 -sS -m 3 "http://$HOST/"
    echo
    exit 0
    ;;
  permission)
    BODY='{"session_id":"'"$SESSION"'","cwd":"'"$CWD"'","hook_event_name":"Notification","message":"Claude needs your permission to use Bash","notification_type":"permission_prompt"}'
    ;;
  idle)
    BODY='{"session_id":"'"$SESSION"'","cwd":"'"$CWD"'","hook_event_name":"Notification","message":"Claude is waiting for your input","notification_type":"idle_prompt"}'
    ;;
  stop)
    BODY='{"session_id":"'"$SESSION"'","cwd":"'"$CWD"'","hook_event_name":"Stop","stop_hook_active":false,"last_assistant_message":"I have finished the **refactor**. All tests pass."}'
    ;;
  clear)
    BODY='{"session_id":"'"$SESSION"'","cwd":"'"$CWD"'","hook_event_name":"UserPromptSubmit"}'
    ;;
  approve)
    BODY='{"session_id":"'"$SESSION"'","cwd":"'"$CWD"'","hook_event_name":"PostToolBatch"}'
    ;;
  big)
    # Stop com ~60 KB de mensagem: o CYD recusa com 413 (máx 16 KB). O hook do
    # exemplo corta com jq antes de mandar.
    FILLER="$(head -c 60000 /dev/zero | tr '\0' 'x')"
    BODY='{"session_id":"'"$SESSION"'","cwd":"'"$CWD"'","hook_event_name":"Stop","last_assistant_message":"Long answer '"$FILLER"'"}'
    ;;
  *)
    echo "uso: $0 permission|idle|stop|clear|approve|big|ping|sleep|wake" >&2
    exit 2
    ;;
esac

printf '%s' "$BODY" | curl -4 -sS -m 3 -o /dev/null -w '%{http_code}\n' \
  -H "Authorization: Bearer ${CYDEX_KEY:-}" \
  -H 'Content-Type: application/json' \
  --data-binary @- "http://$HOST/hook"
