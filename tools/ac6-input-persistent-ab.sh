#!/usr/bin/env bash
# P1.2, second attempt: attribute a PERSISTENT state change to input.
#
# The first attempt compared frame i of a control run against frame i of a
# treatment run. That failed for a measurable reason: the attract sequence is
# not frame-synchronised between runs, so |A-B| between two untouched runs
# reached 116 mean-abs while the treatment effect was 36. Cross-run pixel
# alignment cannot work here.
#
# This discriminator needs no alignment. Within a single run, measure temporal
# activity -- mean |frame_i - frame_{i-1}|. An animated flythrough or attract
# loop has high activity; a menu or a stalled state has near-zero. A press that
# moves the game to a PERSISTENT new state shows up as the activity profile
# diverging after the press and staying diverged, which run-to-run timing jitter
# cannot manufacture.
#
# Input is HELD, never tapped: xdotool key presses and releases in ~12 ms, which
# falls between the guest's polls -- measured in cycle 334.
#
# Usage: ac6-input-persistent-ab.sh [settle] [frames] [interval]
set -uo pipefail

SETTLE="${1:-30}"
FRAMES="${2:-20}"
INTERVAL="${3:-1.5}"
PRESS_AT=8

WT=/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill
TOOLS=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/tools
OUT=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/reports/logs/input-persistent
rm -rf "$OUT"; mkdir -p "$OUT"

hold() { # display, key, seconds
  DISPLAY="$1" xdotool keydown "$2" 2>/dev/null
  sleep "$3"
  DISPLAY="$1" xdotool keyup "$2" 2>/dev/null
}

run() {
  local tag="$1" inject="$2" disp="$3"
  mkdir -p "$OUT/$tag"
  "$TOOLS/ac6-clean-runtime-leaks.sh" >/dev/null 2>&1
  (Xvfb "$disp" -screen 0 1280x720x24 >/dev/null 2>&1 &)
  sleep 3
  ( cd "$WT/build-rt" && DISPLAY="$disp" ./ac6recomp \
      --ac6_performance_mode=false --log_flush_interval=1 --mnk_mode=true \
      --frame_loop_telemetry_interval=300 >"$OUT/$tag/stdout.log" 2>&1 ) &
  sleep "$SETTLE"

  local win
  win="$(DISPLAY="$disp" xdotool search --class ac6recomp 2>/dev/null | head -1)"
  [ -n "$win" ] && DISPLAY="$disp" xdotool windowfocus "$win" 2>/dev/null
  echo "$tag: window=${win:-none}" >>"$OUT/notes.txt"

  for i in $(seq 1 "$FRAMES"); do
    DISPLAY="$disp" import -window root "$OUT/$tag/f$(printf '%02d' "$i").png" 2>/dev/null
    if [ "$inject" = "yes" ] && [ "$i" -eq "$PRESS_AT" ]; then
      hold "$disp" Escape 0.6      # Start
      sleep 0.3
      hold "$disp" space 0.6       # A
      sleep 0.3
      hold "$disp" Escape 0.6      # Start again, in case the first advanced a stage
      echo "$tag: held Escape/space/Escape after frame $i" >>"$OUT/notes.txt"
    fi
    sleep "$INTERVAL"
  done

  cp -f "$WT/build-rt/ac6recomp.log" "$OUT/$tag/ac6recomp.log" 2>/dev/null
  pkill -KILL -x ac6recomp 2>/dev/null; sleep 1
  pkill -KILL -f "Xvfb $disp" 2>/dev/null; sleep 1
}

run C no  :95    # control
run T yes :96    # treatment

echo
echo "=== what the guest actually received (XamInputGetState) ==="
for t in C T; do
  echo "-- run $t --"
  grep -o "\[xam-input\].*" "$OUT/$t/ac6recomp.log" 2>/dev/null | sort -u | head -8
  echo "   nonzero-button lines: $(grep -c '\[xam-input\] guest sees buttons=0x0000' "$OUT/$t/ac6recomp.log" 2>/dev/null | : ; grep -o '\[xam-input\] guest sees buttons=0x[0-9A-F]*' "$OUT/$t/ac6recomp.log" 2>/dev/null | grep -vc 'buttons=0x0000')"
done

python3 - "$OUT" "$FRAMES" "$PRESS_AT" <<'PYEOF'
import sys, os
out, frames, press = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
from PIL import Image
import numpy as np

def load(tag, i):
    p = os.path.join(out, tag, f"f{i:02d}.png")
    return np.asarray(Image.open(p).convert("RGB"), dtype=np.int16) if os.path.exists(p) else None

def activity(tag):
    """mean |frame_i - frame_{i-1}| per frame index."""
    vals = {}
    prev = load(tag, 1)
    for i in range(2, frames + 1):
        cur = load(tag, i)
        if prev is not None and cur is not None and prev.shape == cur.shape:
            vals[i] = float(np.abs(cur - prev).mean())
        prev = cur
    return vals

C, T = activity("C"), activity("T")
print()
print(f"{'frame':>5} {'control activity':>17} {'treatment activity':>19}")
for i in range(2, frames + 1):
    c, t = C.get(i), T.get(i)
    mark = "   <-- input held here" if i == press else ""
    print(f"{i:>5} {c if c is None else f'{c:17.3f}'} {t if t is None else f'{t:19.3f}'}{mark}")

post_c = [v for i, v in C.items() if i > press]
post_t = [v for i, v in T.items() if i > press]
pre_c  = [v for i, v in C.items() if i <= press]
pre_t  = [v for i, v in T.items() if i <= press]
if post_c and post_t and pre_c and pre_t:
    import statistics as st
    print()
    print(f"pre-press  mean activity  control {st.mean(pre_c):8.3f}   treatment {st.mean(pre_t):8.3f}")
    print(f"post-press mean activity  control {st.mean(post_c):8.3f}   treatment {st.mean(post_t):8.3f}")
    # Persistence: does treatment stay different for the rest of the run?
    ratio = st.mean(post_t) / st.mean(post_c) if st.mean(post_c) else float('inf')
    print(f"post/post ratio (T/C)   : {ratio:.2f}x")
    print()
    if ratio < 0.4 or ratio > 2.5:
        print("VERDICT: treatment reached a PERSISTENTLY different regime after the press")
    else:
        print("VERDICT: activity profiles comparable -- no persistent divergence shown")
PYEOF
echo; cat "$OUT/notes.txt" 2>/dev/null
