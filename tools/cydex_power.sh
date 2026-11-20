#!/bin/sh
# Apaga ou acende a tela do CYD junto com o Mac (chamado pelo tools/cydex-power-watch).
#
#   tools/cydex_power.sh sleep|wake
#
# A chave vem de ~/.cydex (o LaunchAgent não herda o ambiente do terminal):
#   CYDEX_KEY=a1b2c3d4
#   CYDEX_HOST=cydex.local   (opcional)
# Variáveis já exportadas valem mais que o arquivo. Sempre sai 0: CYD
# desligado ou fora da rede não é erro.

[ -f "$HOME/.cydex" ] && . "$HOME/.cydex"
HOST="${CYDEX_HOST:-cydex.local}"

case "${1:-}" in
  # O Mac não espera muito antes de dormir: uma tentativa curta.
  sleep) RETRY="" ;;
  # Acordando, o Wi-Fi e o mDNS levam alguns segundos pra voltar.
  wake) RETRY="--retry 10 --retry-delay 2 --retry-all-errors" ;;
  *)
    echo "uso: $0 sleep|wake" >&2
    exit 2
    ;;
esac

# -4: o CYD só tem IPv4 (sem ele o macOS espera 5 s pela resposta IPv6 do mDNS).
# shellcheck disable=SC2086
curl -4 -s -o /dev/null -m 3 $RETRY -X POST \
  -H "Authorization: Bearer ${CYDEX_KEY:-}" "http://$HOST/$1" || true
exit 0
