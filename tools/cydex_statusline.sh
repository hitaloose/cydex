#!/bin/sh
# statusLine do Claude Code -> indicador "quota" do rodapé da Vitrine (14b).
#
# A CLI manda no stdin rate_limits.five_hour e .seven_day (used_percentage e
# resets_at; só em plano Pro/Max, e só depois da 1ª resposta da sessão). Daqui
# saem duas pílulas com o quanto já foi USADO, como o /usage:
# PUT /status/quota "5h 20%" e PUT /status/quota7d "7d 45%" (sem a de 7 dias,
# só a de 5 h). Sem barra; a cor de cada uma: muted até 50%, amarelo até 80%,
# accent acima. O ttl de cada uma é o tempo até a sua janela renovar: depois
# disso o número ficaria velho e a pílula some sozinha.
#
# No settings.json do Claude Code:
#   "statusLine": {"type": "command", "command": "\"$HOME/Development/cydex/tools/cydex_statusline.sh\""}
#
# A linha do terminal: se CYDEX_STATUSLINE_NEXT tiver um comando (a statusline
# que você já usava), ele recebe o mesmo stdin e é ele quem imprime. Senão sai
# uma linha curta: pasta · modelo · 5h 37% · 7d 12%.
#
# Manda no máximo 1x por minuto (carimbo em ~/.cydex.quota), em segundo plano:
# a linha nunca espera o CYD. Chave e endereço como no cydex_claude.sh
# (CYDEX_KEY, CYDEX_HOST, ~/.cydex). CYDEX_DRY=1 imprime o PUT em vez de mandar.

[ -f "$HOME/.cydex" ] && . "$HOME/.cydex"
HOST="${CYDEX_HOST:-cydex.local}"
IN="$(cat)"
STAMP="$HOME/.cydex.quota"

pct() { printf '%s' "$IN" | jq -r "$1 // empty | floor | if . > 100 then 100 elif . < 0 then 0 else . end" 2>/dev/null; }
reset_at() { printf '%s' "$IN" | jq -r "$1 // empty | floor" 2>/dev/null; }

# Uma pílula: put <id> <rótulo> <usado> <resets_at> <ttl sem resets_at>.
put() {
  if [ "$3" -gt 80 ]; then color="#FF5A4E"; elif [ "$3" -gt 50 ]; then color="#F5D547"; else color="#93A0AD"; fi
  ttl=$5
  if [ -n "$4" ]; then
    ttl=$(($4 - now))
    [ "$ttl" -lt 60 ] && ttl=60
    [ "$ttl" -gt 86400 ] && ttl=86400  # máx do firmware: a de 7 dias renova pelo carimbo de 10 min
  fi
  body="{\"text\":\"$2 $3%\",\"text_color\":\"$color\",\"ttl\":$ttl}"
  if [ -n "${CYDEX_DRY:-}" ]; then
    printf 'PUT http://%s/status/%s %s\n' "$HOST" "$1" "$body" >&2
  else
    # -4: o CYD só tem IPv4.
    printf '%s' "$body" | curl -4 -s -m 5 -o /dev/null -X PUT \
      -H "Authorization: Bearer ${CYDEX_KEY:-}" -H "Content-Type: application/json" \
      --data-binary @- "http://$HOST/status/$1"
  fi
}

used="$(pct .rate_limits.five_hour.used_percentage)"
week="$(pct .rate_limits.seven_day.used_percentage)"

if [ -n "$used" ]; then
  state="$used ${week:--}"
  now="$(date +%s)"
  last_s=0
  last_state=""
  [ -f "$STAMP" ] && { read -r last_s _old; read -r last_state; } <"$STAMP" 2>/dev/null
  age=$((now - ${last_s:-0}))
  # 1x/min no máximo; mesmo valor só de 10 em 10 min (CYD reiniciado volta a ver).
  if [ "$age" -ge 60 ] && { [ "$state" != "$last_state" ] || [ "$age" -ge 600 ]; }; then
    [ -z "${CYDEX_DRY:-}" ] && printf '%s\n%s\n' "$now" "$state" >"$STAMP"
    r5="$(reset_at .rate_limits.five_hour.resets_at)"
    r7="$(reset_at .rate_limits.seven_day.resets_at)"
    # As duas em sequência (o CYD atende um cliente por vez), em segundo plano
    # e sem herdar a saída: a linha do terminal não espera o CYD.
    if [ -n "${CYDEX_DRY:-}" ]; then
      put quota 5h "$used" "$r5" 18000
      [ -n "$week" ] && put quota7d 7d "$week" "$r7" 86400
    else
      ( put quota 5h "$used" "$r5" 18000
        [ -n "$week" ] && put quota7d 7d "$week" "$r7" 86400 ) </dev/null >/dev/null 2>&1 &
    fi
  fi
fi

if [ -n "${CYDEX_STATUSLINE_NEXT:-}" ]; then
  printf '%s' "$IN" | sh -c "$CYDEX_STATUSLINE_NEXT"
  exit 0
fi
printf '%s' "$IN" | jq -r '
  [ ((.workspace.current_dir // .cwd // "") | split("/") | last),
    (.model.display_name // empty),
    (.rate_limits.five_hour.used_percentage // empty | "5h \(floor)%"),
    (.rate_limits.seven_day.used_percentage // empty | "7d \(floor)%") ]
  | map(select(. != "")) | join(" · ")' 2>/dev/null
exit 0
