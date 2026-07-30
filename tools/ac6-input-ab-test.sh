#!/usr/bin/env bash
# P1.2 -- does input change the presented state?
#
# Cycle 314 sent Return, space and KP_Enter with xdotool, saw no pixel change,
# and concluded input was dead. Two things were wrong with that test and both
# are corrected here.
#
# First, the keys. The MnK driver binds Start to Escape and A to Space; Return
# and KP_Enter are bound to nothing at all, so that test could not have worked
# whatever the guest did.
#
# Second, and now the harder problem: since the P0 gate opened, the presented
# content ANIMATES. Frames differ from one another with no input at all, so
# "the pixels changed" no longer demonstrates anything. A control is required.
#
# Design: three runs with identical timing.
#   A, B  no input        -> |A-B| is the run-to-run noise floor of the attract
#                            sequence, i.e. how much two untouched runs differ
#   T     input at frame K -> |A-T| after frame K is the candidate effect
#
# Input changed the state only if |A-T| rises decisively above |A-B| after the
# press and stays there. If |A-T| ~ |A-B| throughout, the input did nothing --
# and that verdict is then trustworthy, which cycle 314's was not.
#
# Usage: ac6-input-ab-test.sh [settle_seconds] [frames] [interval]
set -uo pipefail

SETTLE="${1:-30}"
FRAMES="${2:-12}"
INTERVAL="${3:-2}"
PRESS_AT=5   # frame index after which input is injected, 1-based

WT=/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill
TOOLS=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/tools
OUT=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/reports/logs/input-ab
rm -rf "$OUT"; mkdir -p "$OUT"

run() {
  local tag="$1" inject="$2" disp="$3"
  mkdir -p "$OUT/$tag"
  "$TOOLS/ac6-clean-runtime-leaks.sh" >/dev/null 2>&1
  (Xvfb "$disp" -screen 0 1280x720x24 >/dev/null 2>&1 &)
  sleep 3
  ( cd "$WT/build-rt" && DISPLAY="$disp" ./ac6recomp \
      --ac6_performance_mode=false --log_flush_interval=1 \
      --frame_loop_telemetry_interval=300 >"$OUT/$tag/stdout.log" 2>&1 ) &
  sleep "$SETTLE"

  # Without a window manager there is no focus by default; XSetInputFocus via
  # windowfocus is what makes XTEST keystrokes reach the app. Synthetic
  # XSendEvent (xdotool key --window) is deliberately avoided: SDL ignores
  # events with send_event set, so it would silently test nothing.
  local win
  win="$(DISPLAY="$disp" xdotool search --class ac6recomp 2>/dev/null | head -1)"
  [ -z "$win" ] && win="$(DISPLAY="$disp" xdotool search --name . 2>/dev/null | tail -1)"
  if [ -n "$win" ]; then
    DISPLAY="$disp" xdotool windowfocus "$win" 2>/dev/null
    echo "$tag: focused window $win" >>"$OUT/notes.txt"
  else
    echo "$tag: NO WINDOW FOUND" >>"$OUT/notes.txt"
  fi

  for i in $(seq 1 "$FRAMES"); do
    DISPLAY="$disp" import -window root "$OUT/$tag/f$(printf '%02d' "$i").png" 2>/dev/null
    if [ "$inject" = "yes" ] && [ "$i" -eq "$PRESS_AT" ]; then
      # Start, then A -- the two that advance a title screen.
      DISPLAY="$disp" xdotool key Escape 2>/dev/null
      sleep 0.4
      DISPLAY="$disp" xdotool key space 2>/dev/null
      sleep 0.4
      DISPLAY="$disp" xdotool key Escape 2>/dev/null
      echo "$tag: injected Escape+space+Escape after frame $i" >>"$OUT/notes.txt"
    fi
    sleep "$INTERVAL"
  done

  cp -f "$WT/build-rt/ac6recomp.log" "$OUT/$tag/ac6recomp.log" 2>/dev/null
  pkill -KILL -x ac6recomp 2>/dev/null
  sleep 1
  pkill -KILL -f "Xvfb $disp" 2>/dev/null
  sleep 1
}

run A no  :81
run B no  :82
run T yes :83

python3 - "$OUT" "$FRAMES" "$PRESS_AT" <<'PYEOF'
import sys, os
out, frames, press = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
try:
    from PIL import Image
    import numpy as np
except Exception as e:
    print("PIL/numpy unavailable:", e); sys.exit(0)

def load(tag, i):
    p = os.path.join(out, tag, f"f{i:02d}.png")
    if not os.path.exists(p): return None
    return np.asarray(Image.open(p).convert("RGB"), dtype=np.int16)

def diff(a, b):
    if a is None or b is None or a.shape != b.shape: return None
    return float(np.abs(a - b).mean())

print(f"{'frame':>5} {'|A-B| control':>14} {'|A-T| treatment':>16}  verdict-input")
ab, at = [], []
for i in range(1, frames + 1):
    A, B, T = load("A", i), load("B", i), load("T", i)
    d_ab, d_at = diff(A, B), diff(A, T)
    mark = "  <-- input injected here" if i == press else ""
    if d_ab is None or d_at is None:
        print(f"{i:>5} {'n/a':>14} {'n/a':>16}{mark}"); continue
    ab.append((i, d_ab)); at.append((i, d_at))
    print(f"{i:>5} {d_ab:>14.3f} {d_at:>16.3f}{mark}")

pre_ab  = [d for i, d in ab if i <= press]
post_ab = [d for i, d in ab if i >  press]
post_at = [d for i, d in at if i >  press]
if post_ab and post_at:
    noise = max(post_ab)
    eff   = sum(post_at) / len(post_at)
    print()
    print(f"noise floor (max |A-B| after press): {noise:.3f}")
    print(f"mean |A-T| after press             : {eff:.3f}")
    print(f"ratio                              : {eff / noise if noise else float('inf'):.2f}x")
    print()
    print("VERDICT:", "input CHANGED the presented state"
          if eff > 2 * noise else "no effect distinguishable from run-to-run noise")
PYEOF
echo; cat "$OUT/notes.txt" 2>/dev/null
