#!/bin/sh
# Hook do Claude Code -> API de avisos do CYDex (POST/DELETE /notices).
#
# O Claude Code manda o JSON do evento no stdin (tools/claude-hooks.example.json
# chama este script em todos os eventos). O CYD não sabe nada de Claude: aqui
# cada evento vira um aviso ou fecha um.
#
#   PermissionRequest              -> aviso "question" (tag permission) com o que
#                                     o Claude quer: "Rodar git", "Editar nav.cpp"
#   Notification permission_prompt -> o mesmo, genérico (settings antigos)
#   Stop                           -> aviso "success"  (tag done)
#   UserPromptSubmit, SessionEnd   -> fecha o aviso do chat (você respondeu)
#   PostToolBatch, PostToolUse,
#   PostToolUseFailure             -> fecha só o "precisa de permissão" (aprovou)
#
# Em todo evento também atualiza o indicador "claude" do rodapé da Vitrine
# (PUT /status/claude, tela 14b): quantas sessões da CLI estão trabalhando, ou
# em amarelo quantas esperam você. Sem nenhuma, DELETE. Lê ~/.claude/sessions.
#
# Aprovar pelo CYD (tela 15c), desligado de fábrica: o hook PermissionRequest
# síncrono chamando este script com --approve (cydex_install_hooks.sh --approve
# grava assim). É argumento, não variável do ~/.cydex: num hook async a
# decisão seria ignorada.
# Só espera a resposta se você estiver longe do teclado há CYDEX_APPROVE_IDLE
# segundos (padrão 60; 0 = sempre). Aí o aviso vai com actions, o script
# pergunta GET /notices/<id> a cada 1 s por até CYDEX_APPROVE_WAIT s (padrão
# 240) e imprime a decisão. Sem resposta, voltou pro teclado ou o pedido
# mudou: não imprime nada e o terminal pergunta como sempre. Quem tem a chave
# da API passa a poder aprovar comandos: guarde-a.
#
# O id do aviso é o session_id: um aviso por chat, o novo substitui o anterior.
# O título é curto, porque na Vitrine o balão só mostra título + subtítulo. O
# subtítulo é o nome da pasta do projeto.
# Do pedido de permissão só sai o que diz o que ele toca: o programa (nunca os
# argumentos), o nome do arquivo, o host da URL ou o servidor MCP. O texto do
# prompt nunca sai daqui. Precisa de jq (vem no macOS) e curl.
#
# Chave e endereço: CYDEX_KEY e CYDEX_HOST (padrão cydex.local), do ambiente
# ou de ~/.cydex. CYDEX_LANG=en troca os textos pro inglês. CYDEX_DRY=1 só
# imprime o que mandaria. Sempre sai 0: CYD desligado ou fora da rede não pode
# atrapalhar o Claude.

[ -f "$HOME/.cydex" ] && . "$HOME/.cydex"
APPROVE=0
[ "${1:-}" = "--approve" ] && APPROVE=1
HOST="${CYDEX_HOST:-cydex.local}"
IN="$(cat)"

field() { printf '%s' "$IN" | jq -r "$1 // \"\"" 2>/dev/null; }
EVENT="$(field .hook_event_name)"
SID="$(field .session_id)"
# Sem sessão não dá pra casar o aviso com quem o fecha.
[ -n "$SID" ] || exit 0

# Sessões de fundo de outras ferramentas (claude-mem escrevendo memória,
# CodexBar lendo a cota) não são conversa de ninguém.
case "$(field .cwd)" in
  */.claude-mem/observer-sessions/* | */CodexBar/ClaudeProbe | */CodexBar/ClaudeProbe/) exit 0 ;;
esac

if [ "${CYDEX_LANG:-pt-BR}" = "en" ]; then
  DENY_MSG="Denied on the CYDex."
  LANG_JSON='{"perm":"Permission","done":"Done","run":"Run","cmd":"Run a command",
    "edit":"Edit","write":"Write","read":"Read","file":"a file","open":"Open",
    "web":"Search the web","find":"Find files","ask":"Question","use":"Use","mcp":"MCP"}'
else
  DENY_MSG="Negado no CYDex."
  LANG_JSON='{"perm":"Permissão","done":"Terminou","run":"Rodar","cmd":"Rodar comando",
    "edit":"Editar","write":"Gravar","read":"Ler","file":"arquivo","open":"Abrir",
    "web":"Buscar na web","find":"Buscar arquivos","ask":"Pergunta","use":"Usar","mcp":"MCP"}'
fi

# -4: o CYD só tem IPv4 (sem ele o macOS espera 5 s pela resposta IPv6 do mDNS).
send() {
  if [ -n "${CYDEX_DRY:-}" ]; then
    # Último argumento é a URL; o corpo, se houver, vem no stdin.
    for a; do url="$a"; done
    case " $* " in *" --data-binary @- "*) printf '%s %s\n' "$url" "$(cat)" ;; *) printf '%s %s\n' "$*" "" ;; esac
    return 0
  fi
  curl -4 -s -m 5 -o /dev/null -H "Authorization: Bearer ${CYDEX_KEY:-}" "$@" || true
}

# O que o pedido toca, em poucas palavras. Regras do phonebox
# (ClaudeTranscript.Program): o 1º programa da linha, depois de cd, sudo, env,
# VAR=valor e afins; com $( ), ${ }, crase, barra invertida ou aspas sem fechar
# não dá pra saber o que roda, e não sai nada.
JQ_LIB='
def base: sub("[/\\\\]+$"; "") | split("/") | last // "" | split("\\") | last // "";
def program:
  if (type != "string") or test("`|\\$[({]|\\\\") then null
  else
    gsub("\\x27[^\\x27]*\\x27"; "\u0001") | gsub("\"[^\"]*\""; "\u0001")
    | if test("[\\x27\"]") then null
      else
        [ gsub("[(){}]"; " ") | splits("[;&|\n]+")
          | [ splits("[ \t]+") | select(length > 0) ] | select(length > 0) ]
        | first(
            .[] as $seg
            | label $out
            | foreach $seg[] as $w ({done: false};
                if .done then .
                elif ($w | test("^[A-Za-z_][A-Za-z0-9_]*=")) then .
                elif ($w | IN("cd", "export", "set", "unset", "source", ".", "for", "if", "while", "case")) then .done = true | .skip = true
                elif ($w | IN("sudo", "env", "time", "nohup", "exec", "command", "do", "then", "else", "rtk")) then .
                elif ($w | startswith("-")) then .done = true | .result = "?"
                elif ($w | test("^[A-Za-z0-9._/-]{1,40}$")) then .done = true | .result = ($w | base)
                else .done = true | .result = "?" end;
                if .done then (if .skip then break $out else .result end) else empty end)
          ) // null
        | if . == "?" or . == "" then null else . end
      end
  end;
def host: if type == "string" then (capture("^[a-zA-Z][a-zA-Z0-9+.-]*://(?<h>[^/:?#@]+)") | .h) // null else null end;
def mcp_server: ltrimstr("mcp__") | split("__") | first | sub("^(claude_ai_|plugin_[^_]+_)"; "");
def ask_title($t):
  .tool_name as $n | (.tool_input // {}) as $i
  | ($i.file_path // $i.notebook_path // null | if . then base else null end) as $f
  | if $n == "Bash" then ($i.command | program | if . then "\($t.run) \(.)" else $t.cmd end)
    elif $n == "Edit" or $n == "MultiEdit" or $n == "NotebookEdit" then "\($t.edit) \($f // $t.file)"
    elif $n == "Write" then "\($t.write) \($f // $t.file)"
    elif $n == "Read" then "\($t.read) \($f // $t.file)"
    elif $n == "WebFetch" then ($i.url | host | if . then "\($t.open) \(.)" else $t.open end)
    elif $n == "WebSearch" then $t.web
    elif $n == "Glob" or $n == "Grep" then $t.find
    elif $n == "AskUserQuestion" then $t.ask
    elif ($n // "" | startswith("mcp__")) then "\($t.mcp) \($n | mcp_server)"
    elif ($n // "") != "" then "\($t.use) \($n)"
    else $t.perm end;
def ask_message:
  .tool_name as $n | (.tool_input // {}) as $i
  | if $n == "AskUserQuestion" then ($i.questions[0].question // "")
    else ($i.description // "") end;
'

# Corpo do aviso: tipo, tag, título e mensagem já prontos; $5 = segundos de
# timeout com actions allow/deny (só no modo de aprovação).
notice_body() {
  printf '%s' "$IN" | jq -c "$JQ_LIB"'
      {
        id: .session_id, tag: $tag, type: $type, title: $title,
        subtitle: ((.cwd // "") | base),
        message: ($msg | tostring | .[0:400])
      }
      + (if $wait != "" then {actions: ["allow", "deny"], timeout: ($wait | tonumber)} else {} end)' \
    --arg type "$1" --arg tag "$2" --arg title "$3" --arg msg "$4" --arg wait "${5:-}"
}

post() {
  notice_body "$@" | send -H "Content-Type: application/json" --data-binary @- "http://$HOST/notices"
}

# Indicador "claude" (14b). Sessões vivas da CLI (pid do nome do arquivo),
# sem as de fundo. A deste evento usa o estado que o evento diz ($1), porque
# o arquivo da sessão ainda pode não ter mudado.
STAMP="$HOME/.cydex.claude-chip"
claude_chip() {
  dir="$HOME/.claude/sessions"
  busy=0
  wait=0
  for f in "$dir"/*.json; do
    [ -f "$f" ] || continue
    pid="${f##*/}"
    pid="${pid%.json}"
    kill -0 "$pid" 2>/dev/null || continue
    st="$(jq -r --arg sid "$SID" --arg now "$1" '
      if (.kind // "interactive") != "interactive" then "skip"
      elif ((.cwd // "") | test("/\\.claude-mem/observer-sessions/|/CodexBar/ClaudeProbe/?$")) then "skip"
      elif .sessionId == $sid then $now
      elif ((.waitingFor // "") != "") or .status == "waiting" then "waiting"
      else (.status // "idle") end' "$f" 2>/dev/null)"
    case "$st" in
      waiting) wait=$((wait + 1)) ;;
      busy) busy=$((busy + 1)) ;;
    esac
  done
  if [ "$wait" -gt 0 ]; then
    state="w$wait"
    body="{\"text\":\"$wait\",\"icon\":\"terminal\",\"color\":\"#F5D547\",\"text_color\":\"#F5D547\",\"ttl\":1800}"
  elif [ "$busy" -gt 0 ]; then
    state="b$busy"
    body="{\"text\":\"$busy\",\"icon\":\"terminal\",\"ttl\":1800}"
  else
    state="none"
    body=""
  fi
  # Mesmo estado há menos de 10 min: o CYD já mostra isso (o ttl cobre 30).
  now_s="$(date +%s)"
  if [ -f "$STAMP" ] && [ -z "${CYDEX_DRY:-}" ]; then
    read -r last_state last_s <"$STAMP" 2>/dev/null || true
    if [ "${last_state:-}" = "$state" ] && [ $((now_s - ${last_s:-0})) -lt 600 ]; then return 0; fi
  fi
  [ -n "${CYDEX_DRY:-}" ] || printf '%s %s\n' "$state" "$now_s" >"$STAMP"
  if [ -n "$body" ]; then
    printf '%s' "$body" | send -X PUT -H "Content-Type: application/json" --data-binary @- "http://$HOST/status/claude"
  else
    send -X DELETE "http://$HOST/status/claude"
  fi
}

# Segundos sem teclado nem mouse (macOS). Sem ioreg: "longe" (espera sempre).
idle_s() {
  ioreg -c IOHIDSystem 2>/dev/null | awk '/HIDIdleTime/ {print int($NF / 1000000000); exit}'
}

# Aprovação pelo CYD (15c): POST com actions, depois pergunta a cada 1 s.
# Imprime a decisão no formato do PermissionRequest ou nada.
approve() {
  wait_s="${CYDEX_APPROVE_WAIT:-240}"
  resp="$(notice_body question permission "$1" "$2" "$wait_s" \
    | curl -4 -s -m 3 -H "Authorization: Bearer ${CYDEX_KEY:-}" -H "Content-Type: application/json" \
        --data-binary @- "http://$HOST/notices" 2>/dev/null)"
  rev="$(printf '%s' "$resp" | jq -r '.rev // 0' 2>/dev/null)"
  # Firmware sem resposta (api < 4) ou o aviso não entrou: o terminal pergunta.
  [ -n "$rev" ] && [ "$rev" != "0" ] || return 0
  # A CLI matou o hook (você respondeu no terminal): o balão sai.
  trap 'send -X DELETE "http://$HOST/notices/$SID?tag=permission"; exit 0' HUP INT TERM
  end=$(($(date +%s) + wait_s))
  while [ "$(date +%s)" -lt "$end" ]; do
    sleep 1
    got="$(curl -4 -s -m 2 -H "Authorization: Bearer ${CYDEX_KEY:-}" "http://$HOST/notices/$SID" 2>/dev/null)"
    [ -n "$got" ] || continue  # CYD não respondeu desta vez
    grev="$(printf '%s' "$got" | jq -r '.rev // 0' 2>/dev/null)"
    ans="$(printf '%s' "$got" | jq -r '.answer // ""' 2>/dev/null)"
    # Fechado (404 sem corpo JSON), expirado ou trocado por outro pedido.
    [ "$grev" = "$rev" ] || return 0
    case "$ans" in
      allow)
        printf '%s\n' '{"hookSpecificOutput":{"hookEventName":"PermissionRequest","decision":{"behavior":"allow"}}}'
        return 0 ;;
      deny)
        jq -nc --arg m "$DENY_MSG" '{hookSpecificOutput:{hookEventName:"PermissionRequest",decision:{behavior:"deny",message:$m}}}'
        return 0 ;;
    esac
    # Voltou pro teclado: responde lá (com CYDEX_APPROVE_IDLE=0, espera sempre).
    if [ "${CYDEX_APPROVE_IDLE:-60}" -gt 0 ]; then
      i="$(idle_s)"
      if [ -n "$i" ] && [ "$i" -lt 3 ]; then return 0; fi
    fi
  done
}

t() { printf '%s' "$LANG_JSON" | jq -r ".$1"; }

# No modo de aprovação o PermissionRequest é síncrono: o que não decide nada
# vai pra segundo plano (CYD fora da rede não pode atrasar o terminal).
SYNC=0
[ "$EVENT" = PermissionRequest ] && [ "$APPROVE" = 1 ] && [ -z "${CYDEX_DRY:-}" ] && SYNC=1
bg() { if [ "$SYNC" = 1 ]; then "$@" </dev/null >/dev/null 2>&1 & else "$@"; fi; }

case "$EVENT" in
  PermissionRequest) bg claude_chip waiting ;;
  Notification) [ "$(field .notification_type)" = permission_prompt ] && claude_chip waiting ;;
  UserPromptSubmit | PostToolBatch | PostToolUse | PostToolUseFailure) claude_chip busy ;;
  Stop | SessionEnd) claude_chip idle ;;
esac

case "$EVENT" in
  PermissionRequest)
    TITLE="$(printf '%s' "$IN" | jq -r --argjson t "$LANG_JSON" "$JQ_LIB"' ask_title($t)' 2>/dev/null)"
    MSG="$(printf '%s' "$IN" | jq -r "$JQ_LIB"' ask_message' 2>/dev/null)"
    TITLE="${TITLE:-$(t perm)}"
    away=0
    if [ "$SYNC" = 1 ]; then
      i="$(idle_s)"
      if [ -z "$i" ] || [ "$i" -ge "${CYDEX_APPROVE_IDLE:-60}" ]; then away=1; fi
    fi
    if [ "$away" = 1 ]; then approve "$TITLE" "$MSG"; else bg post question permission "$TITLE" "$MSG"; fi
    ;;
  Notification)
    case "$(field .notification_type)" in
      permission_prompt) post question permission "$(t perm)" "$(field .message)" ;;
    esac
    ;;
  Stop) post success done "$(t done)" "$(field .last_assistant_message)" ;;
  UserPromptSubmit | SessionEnd) send -X DELETE "http://$HOST/notices/$SID" ;;
  PostToolBatch | PostToolUse | PostToolUseFailure) send -X DELETE "http://$HOST/notices/$SID?tag=permission" ;;
esac
exit 0
