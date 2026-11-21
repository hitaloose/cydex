#!/bin/sh
# Hook do Claude Code -> API de avisos do CYDex (POST/DELETE /notices).
#
# O Claude Code manda o JSON do evento no stdin (tools/claude-hooks.example.json
# chama este script em todos os eventos). O CYD não sabe nada de Claude: aqui
# cada evento vira um aviso ou fecha um.
#
#   Notification permission_prompt -> aviso "question" (tag permission)
#   Notification idle_prompt       -> aviso "waiting"  (tag idle)
#   Stop                           -> aviso "success"  (tag done)
#   UserPromptSubmit, SessionEnd   -> fecha o aviso do chat (você respondeu)
#   PostToolBatch, PostToolUse,
#   PostToolUseFailure             -> fecha só o "precisa de permissão" (aprovou)
#
# O id do aviso é o session_id: um aviso por chat, o novo substitui o anterior.
# O texto do prompt nunca sai daqui. Precisa de jq (vem no macOS) e curl.
#
# Chave e endereço: CYDEX_KEY e CYDEX_HOST (padrão cydex.local), do ambiente
# ou de ~/.cydex. CYDEX_LANG=en troca os títulos pro inglês. Sempre sai 0:
# CYD desligado ou fora da rede não pode atrapalhar o Claude.

[ -f "$HOME/.cydex" ] && . "$HOME/.cydex"
HOST="${CYDEX_HOST:-cydex.local}"
IN="$(cat)"

field() { printf '%s' "$IN" | jq -r "$1 // \"\"" 2>/dev/null; }
EVENT="$(field .hook_event_name)"
SID="$(field .session_id)"
# Sem sessão não dá pra casar o aviso com quem o fecha.
[ -n "$SID" ] || exit 0

if [ "${CYDEX_LANG:-pt-BR}" = "en" ]; then
  T_PERM="Claude needs permission"; T_IDLE="Claude is waiting for you"; T_DONE="Claude is done"
else
  T_PERM="Claude precisa de permissão"; T_IDLE="Claude está esperando você"; T_DONE="Claude terminou"
fi

# -4: o CYD só tem IPv4 (sem ele o macOS espera 5 s pela resposta IPv6 do mDNS).
send() {
  curl -4 -s -m 5 -o /dev/null -H "Authorization: Bearer ${CYDEX_KEY:-}" "$@" || true
}

# POST do aviso: tipo, tag, título e de onde vem a mensagem.
notify() {
  printf '%s' "$IN" | jq -c --arg type "$1" --arg tag "$2" --arg title "$3" --arg msg "$4" '{
      id: .session_id, tag: $tag, type: $type, title: $title,
      subtitle: ((.cwd // "") | sub("[/\\\\]+$"; "") | (split("/") | last // "") | (split("\\") | last // "")),
      message: ((getpath($msg | split(".")) // "") | tostring | .[0:400])
    }' | send -H "Content-Type: application/json" --data-binary @- "http://$HOST/notices"
}

case "$EVENT" in
  Notification)
    case "$(field .notification_type)" in
      permission_prompt) notify question permission "$T_PERM" message ;;
      idle_prompt) notify waiting idle "$T_IDLE" message ;;
    esac
    ;;
  Stop) notify success done "$T_DONE" last_assistant_message ;;
  UserPromptSubmit | SessionEnd) send -X DELETE "http://$HOST/notices/$SID" ;;
  PostToolBatch | PostToolUse | PostToolUseFailure) send -X DELETE "http://$HOST/notices/$SID?tag=permission" ;;
esac
exit 0
