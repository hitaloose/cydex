#!/bin/sh
# statusLine do Claude Code -> indicador "quota" do rodapé da Vitrine (14b).
#
# A CLI manda no stdin rate_limits.five_hour e .seven_day (used_percentage e
# resets_at; só em plano Pro/Max, e só depois da 1ª resposta da sessão). Daqui
# sai um PUT /status/quota com o quanto já foi USADO das duas janelas, como o
# /usage: "5h 20% | 7d 45%" (sem a de 7 dias, só "5h 20%"). Sem barra; a cor
# do texto é a da janela mais usada: muted até 50%, amarelo até 80%, accent
# acima. O ttl é o tempo até a janela de 5 h renovar: depois disso o número
# ficaria velho e o chip some sozinho.
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
used="$(pct .rate_limits.five_hour.used_percentage)"
week="$(pct .rate_limits.seven_day.used_percentage)"
resets="$(printf '%s' "$IN" | jq -r '.rate_limits.five_hour.resets_at // empty | floor' 2>/dev/null)"

if [ -n "$used" ]; then
  text="5h $used%"
  worst=$used
  if [ -n "$week" ]; then
    text="$text | 7d $week%"
    [ "$week" -gt "$worst" ] && worst=$week
  fi
  now="$(date +%s)"
  last_s=0
  last_text=""
  [ -f "$STAMP" ] && { read -r last_s _old; read -r last_text; } <"$STAMP" 2>/dev/null
  age=$((now - ${last_s:-0}))
  # 1x/min no máximo; mesmo valor só de 10 em 10 min (CYD reiniciado volta a ver).
  if [ "$age" -ge 60 ] && { [ "$text" != "$last_text" ] || [ "$age" -ge 600 ]; }; then
    if [ "$worst" -gt 80 ]; then color="#FF5A4E"; elif [ "$worst" -gt 50 ]; then color="#F5D547"; else color="#93A0AD"; fi
    ttl=18000  # sem resets_at: a janela inteira
    if [ -n "$resets" ]; then
      ttl=$((resets - now))
      [ "$ttl" -lt 60 ] && ttl=60
      [ "$ttl" -gt 86400 ] && ttl=86400
    fi
    body="{\"text\":\"$text\",\"text_color\":\"$color\",\"ttl\":$ttl}"
    if [ -n "${CYDEX_DRY:-}" ]; then
      printf 'PUT http://%s/status/quota %s\n' "$HOST" "$body" >&2
    else
      printf '%s\n%s\n' "$now" "$text" >"$STAMP"
      # -4: o CYD só tem IPv4. Fundo e sem herdar a saída: a linha não espera.
      (printf '%s' "$body" | curl -4 -s -m 5 -o /dev/null -X PUT \
        -H "Authorization: Bearer ${CYDEX_KEY:-}" -H "Content-Type: application/json" \
        --data-binary @- "http://$HOST/status/quota") </dev/null >/dev/null 2>&1 &
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
