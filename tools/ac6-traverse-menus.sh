#!/usr/bin/env bash
# P1.3 -- drive the game from the title screen toward the campaign selector.
#
# Uses what cycles 334/335 established about the input path, all of which are
# necessary and none of which is obvious:
#   --mnk_mode=true   the MnK driver is OFF by default and reports
#                     DEVICE_NOT_CONNECTED, so the guest sees no pad at all
#   Escape = Start, Space = A, arrows = D-pad
#   keys must be HELD (keydown, pause, keyup); xdotool key presses and releases
#   in ~12 ms and falls between the guest's polls
#
# Strategy: press Start to leave the title, then advance with A, capturing a
# frame before and after every action so the traversal is auditable step by
# step rather than as a single before/after pair. The guest's own button
# receipts are pulled from the log at the end, so a step that did nothing is
# distinguishable from a step whose input never arrived.
#
# Usage: ac6-traverse-menus.sh [settle] [label]
set -uo pipefail

SETTLE="${1:-32}"
LABEL="${2:-p13}"
DISP=:99

WT=/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill
TOOLS=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/tools
OUT=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/reports/logs/traverse-$LABEL
rm -rf "$OUT"; mkdir -p "$OUT"

step=0
shot() { step=$((step+1)); DISPLAY=$DISP import -window root \
         "$OUT/$(printf '%02d' $step)-$1.png" 2>/dev/null; echo "  shot $(printf '%02d' $step)-$1"; }
hold() { DISPLAY=$DISP xdotool keydown "$1" 2>/dev/null; sleep "${2:-0.5}"
         DISPLAY=$DISP xdotool keyup "$1" 2>/dev/null; echo "  held $1 ${2:-0.5}s"; }

"$TOOLS/ac6-clean-runtime-leaks.sh" >/dev/null 2>&1
(Xvfb $DISP -screen 0 1280x720x24 >/dev/null 2>&1 &)
sleep 3
( cd "$WT/build-rt" && DISPLAY=$DISP ./ac6recomp \
    --ac6_performance_mode=false --log_flush_interval=1 --mnk_mode=true \
    --frame_loop_telemetry_interval=300 >"$OUT/stdout.log" 2>&1 ) &
sleep "$SETTLE"

win="$(DISPLAY=$DISP xdotool search --class ac6recomp 2>/dev/null | head -1)"
[ -n "$win" ] && DISPLAY=$DISP xdotool windowfocus "$win" 2>/dev/null
echo "window=${win:-none}"

shot title
# Leave the title. The attract loop freezes without input (measured, cycle 335),
# so this is also the step that keeps the game alive at all.
hold Escape 0.6; sleep 3; shot after-start
hold space  0.6; sleep 3; shot after-a1
hold space  0.6; sleep 3; shot after-a2
hold Escape 0.6; sleep 3; shot after-start2
hold space  0.6; sleep 3; shot after-a3
# Try moving a selection, which is what a menu (and only a menu) responds to.
hold Down   0.4; sleep 2; shot after-down
hold Up     0.4; sleep 2; shot after-up
hold space  0.6; sleep 4; shot after-a4
sleep 5;             shot settled

cp -f "$WT/build-rt/ac6recomp.log" "$OUT/ac6recomp.log" 2>/dev/null
pkill -KILL -x ac6recomp 2>/dev/null; sleep 1
pkill -KILL -f "Xvfb $DISP" 2>/dev/null

echo
echo "=== buttons the guest actually received ==="
grep -o "\[xam-input\] guest sees buttons=0x[0-9A-F]*" "$OUT/ac6recomp.log" 2>/dev/null \
  | sort | uniq -c | sort -rn | head -10

echo
echo "=== per-step change (mean abs vs previous shot) ==="
python3 - "$OUT" <<'PYEOF'
import sys, os, glob
from PIL import Image
import numpy as np
out = sys.argv[1]
fs = sorted(glob.glob(os.path.join(out, "*.png")))
prev, prev_name = None, None
for f in fs:
    a = np.asarray(Image.open(f).convert("RGB"), dtype=np.int16)
    name = os.path.basename(f)
    if prev is not None and prev.shape == a.shape:
        print(f"  {prev_name:>24} -> {name:<24} {float(np.abs(a-prev).mean()):8.3f}")
    prev, prev_name = a, name
PYEOF
echo
echo "frames in $OUT"
