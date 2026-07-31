#!/usr/bin/env bash
# Drive AC6 to the save screen by observing state, with two corrections learned
# the hard way.
#
# 1. Focus is re-asserted before EVERY press. A single windowfocus at startup is
#    not enough: a run was observed where all four captures were byte-identical
#    and no input took effect.
# 2. Stability alone is not the signal. A screen can be stable because nothing
#    is happening, which is exactly the failure above. Each press must be
#    followed by a CHANGE; if the frame does not move, the press did not land
#    and is retried.
#
# Usage: ac6-drive-to-save.sh <display> <outdir> [extra ac6recomp args...]
set -uo pipefail
D="${1:?display}"; OUT="${2:?outdir}"; shift 2
WT=/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill
TOOLS=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/tools
mkdir -p "$OUT"; export DISPLAY="$D"

grab(){ import -window root "$1" 2>/dev/null; }
delta(){ python3 -c "
from PIL import Image; import numpy as np,sys
a=np.asarray(Image.open(sys.argv[1]).convert('RGB'),dtype=np.int16)
b=np.asarray(Image.open(sys.argv[2]).convert('RGB'),dtype=np.int16)
print(int((np.abs(a-b).sum(axis=2)>12).sum()))" "$1" "$2" 2>/dev/null || echo 0; }
focus(){ local w; w=$(xdotool search --class ac6recomp 2>/dev/null | head -1)
         [ -n "$w" ] && { xdotool windowfocus "$w" 2>/dev/null; xdotool windowactivate "$w" 2>/dev/null; }; }

# Press until the screen actually moves. Returns 0 on observed change.
press_until_change(){
  local key="$1" tries="${2:-6}" i d
  for i in $(seq 1 "$tries"); do
    grab "$OUT/.before.png"; focus
    xdotool keydown "$key"; sleep 0.5; xdotool keyup "$key"
    sleep 2; grab "$OUT/.after.png"
    d=$(delta "$OUT/.before.png" "$OUT/.after.png")
    if [ "$d" -gt 2000 ]; then echo "  $key -> changed ($d px) on try $i"; return 0; fi
    echo "  $key -> no change ($d px), retry $i"
  done
  echo "  $key -> never produced a change"; return 1
}

"$TOOLS/ac6-clean-runtime-leaks.sh" >/dev/null 2>&1 || true
( cd "$WT/build-rt" && ./ac6recomp --ac6_performance_mode=false --log_flush_interval=1 \
    --mnk_mode=true "$@" >"$OUT/stdout.log" 2>&1 ) &
sleep 20; focus
grab "$OUT/a-start.png"

echo "step 1: A to skip the intro"; press_until_change space 8 || true; grab "$OUT/b-title.png"
echo "step 2: Start";              press_until_change Escape 6 || true; grab "$OUT/c-post-start.png"
echo "step 3: A";                  press_until_change space 6 || true; grab "$OUT/d-after-a1.png"
echo "step 4: A";                  press_until_change space 6 || true; grab "$OUT/e-save.png"

cp -f "$WT/build-rt/ac6recomp.log" "$OUT/log.txt" 2>/dev/null
pkill -9 -x ac6recomp 2>/dev/null; rm -f "$OUT/.before.png" "$OUT/.after.png"
