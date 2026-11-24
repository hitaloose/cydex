#!/bin/sh
# statusLine do Claude Code -> indicador "quota" do rodapé da Vitrine (14b).
#
# A CLI manda no stdin rate_limits.five_hour.used_percentage e .resets_at (só
# em plano Pro/Max, e só depois da 1ª resposta da sessão). Daqui sai um
# PUT /status/quota com "5h" e uma barra do que SOBRA da janela, nas cores da
# barra de PS do Time: verde > 50%, amarelo > 20%, accent. O ttl é o tempo até
# a janela renovar: depois disso o chip some sozinho.
#
# No settings.json do Claude Code:
#   "statusLine": {"type": "command", "command": "\"$HOME/Development/cydex/tools/cydex_statusline.sh\""}
#
# A linha do terminal: se CYDEX_STATUSLINE_NEXT tiver um comando (a statusline
# que você já usava), ele recebe o mesmo stdin e é ele quem imprime. Senão sai
# uma linha curta: pasta · modelo · 5h 37%.
#
# Manda no máximo 1x por minuto (carimbo em ~/.cydex.quota), em segundo plano:
# a linha nunca espera o CYD. Chave e endereço como no cydex_claude.sh
# (CYDEX_KEY, CYDEX_HOST, ~/.cydex). CYDEX_DRY=1 imprime o PUT em vez de mandar.

[ -f "$HOME/.cydex" ] && . "$HOME/.cydex"
HOST="${CYDEX_HOST:-cydex.local}"
IN="$(cat)"
STAMP="$HOME/.cydex.quota"

used="$(printf '%s' "$IN" | jq -r '.rate_limits.five_hour.used_percentage // empty | floor' 2>/dev/null)"
resets="$(printf '%s' "$IN" | jq -r '.rate_limits.five_hour.resets_at // empty | floor' 2>/dev/null)"

if [ -n "$used" ]; then
  [ "$used" -gt 100 ] && used=100
  [ "$used" -lt 0 ] && used=0
  left=$((100 - used))
  now="$(date +%s)"
  last_s=0
  last_left=""
  [ -f "$STAMP" ] && read -r last_s last_left <"$STAMP" 2>/dev/null
  age=$((now - ${last_s:-0}))
  # 1x/min no máximo; mesmo valor só de 10 em 10 min (CYD reiniciado volta a ver).
  if [ "$age" -ge 60 ] && { [ "$left" != "$last_left" ] || [ "$age" -ge 600 ]; }; then
    if [ "$left" -gt 50 ]; then color="#6FCF6A"; elif [ "$left" -gt 20 ]; then color="#F5D547"; else color="#FF5A4E"; fi
    ttl=18000  # sem resets_at: a janela inteira
    if [ -n "$resets" ]; then
      ttl=$((resets - now))
      [ "$ttl" -lt 60 ] && ttl=60
      [ "$ttl" -gt 86400 ] && ttl=86400
    fi
    body="{\"text\":\"5h\",\"text_color\":\"#93A0AD\",\"color\":\"$color\",\"bar\":$left,\"ttl\":$ttl}"
    if [ -n "${CYDEX_DRY:-}" ]; then
      printf 'PUT http://%s/status/quota %s\n' "$HOST" "$body" >&2
    else
      printf '%s %s\n' "$now" "$left" >"$STAMP"
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
    (.rate_limits.five_hour.used_percentage // empty | "5h \(floor)%") ]
  | map(select(. != "")) | join(" · ")' 2>/dev/null
exit 0
