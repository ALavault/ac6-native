#!/usr/bin/env bash
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
AC6_TREE="$ROOT/.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill"
OUT=""
DISPLAY_ID=":95"
DURATION=240
ATTEMPTS=3

while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --display) DISPLAY_ID="$2"; shift 2 ;;
    --duration) DURATION="$2"; shift 2 ;;
    --attempts) ATTEMPTS="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

[ -n "$OUT" ] || {
  echo "usage: $0 --out DIR [--display :NN] [--duration SEC]" >&2
  exit 2
}
OUT="$(readlink -m "$OUT")"

mkdir -p "$OUT"
last_status=1
for attempt in $(seq 1 "$ATTEMPTS"); do
  attempt_out="$OUT/attempt-$attempt"
  echo "first-mission attempt $attempt/$ATTEMPTS"
  if "$AC6_TREE/tools/ac6-run.sh" \
      --out "$attempt_out" \
      --duration "$DURATION" \
      --display "$DISPLAY_ID" \
      --capture-at 0 \
      --startup-timeout 120 \
      --keys "0:Escape:0.1,2:space:0.1" \
      --wait-for 'type28=30' \
      --wait-pulse 'Escape+space:0.1:2' \
      --wait-stall-timeout 20 \
      --step-file "$ROOT/workspaces/ace-combat-6/scripts/ac6-first-mission.steps" \
      -- \
      --ac6_log_ui_dispatch=true \
      --user_data_root="$attempt_out/user-data"; then
    cp "$attempt_out/ac6recomp.log" "$OUT/ac6recomp.log"
    cp "$attempt_out/run.log" "$OUT/run.log"
    for artifact in "$attempt_out"/*.png; do
      [ -f "$artifact" ] || continue
      cp "$artifact" "$OUT/"
    done
    exit 0
  else
    last_status=$?
  fi
done
exit "$last_status"
