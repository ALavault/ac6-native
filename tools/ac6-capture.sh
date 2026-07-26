#!/usr/bin/env bash
# Launch ac6recomp on a private Xvfb display and capture the root window at
# 15 s intervals. Prints, per capture, the mean pixel value and the number of
# distinct colours -- an all-black screen scores mean=0, distinct_colours=1.
#
# Everything is timeout-guarded: a hung capture must not stall the experiment.
set -uo pipefail
W=/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill
OUT="$1"; shift          # output directory
DUR="$1"; shift          # seconds to run the game
DISP="${DISPLAY_NUM:-:77}"
mkdir -p "$OUT"; rm -f "$OUT"/*.png

pkill -x ac6recomp 2>/dev/null
pkill -f "Xvfb $DISP" 2>/dev/null
sleep 1

nohup Xvfb "$DISP" -screen 0 1920x1080x24 >/dev/null 2>&1 &
sleep 3
pgrep -f "Xvfb $DISP" >/dev/null || { echo "FATAL: Xvfb did not start"; exit 1; }

cd "$W" || exit 1
DISPLAY="$DISP" nohup timeout -k 5 "$DUR" ./build-rt/ac6recomp "$@" > "$OUT/run.log" 2>&1 &

t=0
while [ "$t" -lt $((DUR - 10)) ]; do
  sleep 15
  t=$((t + 15))
  if DISPLAY="$DISP" timeout 20 import -window root "$OUT/t${t}.png" 2>/dev/null; then
    echo "captured t=${t}s  (game alive: $(pgrep -xc ac6recomp || echo 0))"
  else
    echo "capture FAILED at t=${t}s"
  fi
done

sleep 15
pkill -x ac6recomp 2>/dev/null
sleep 2
pkill -f "Xvfb $DISP" 2>/dev/null

echo "--- captures ---"
for f in "$OUT"/t*.png; do
  [ -f "$f" ] || continue
  mean=$(timeout 30 convert "$f" -format "%[fx:mean]" info: 2>/dev/null)
  colors=$(timeout 30 convert "$f" -format "%k" info: 2>/dev/null)
  printf "%-10s mean=%-22s distinct_colours=%s\n" "$(basename "$f")" "${mean:-?}" "${colors:-?}"
done
