#!/usr/bin/env bash
# Launch ac6recomp headless on a private X display, optionally drive it with a
# scripted key sequence, and capture frames on a schedule.
#
# The harness that produced every runtime measurement in cycles 3xx-452 lived
# outside this repository, in a scratch directory, hardcoding one worktree path.
# Results taken with it were not reproducible by anyone who did not also have
# that directory. This is that harness, in the tree, parameterised.
#
# Usage:
#   tools/ac6-run.sh --out DIR --duration 90 [options] [-- extra ac6recomp args]
#
#   --out DIR         output directory (created, PNGs cleared)
#   --duration SEC    how long to run the game
#   --keys SEQ        comma-separated  AT:KEY:HOLD  triples, seconds:
#                       --keys "20:Left:0.6,25:space:0.6"
#                     KEY is an X keysym. The MnK bindings that matter:
#                       A = space   B = shift   Start = Escape
#                       D-pad = Up / Down / Left / Right
#                     HOLD is critical and 0.1 is the right value. A bare
#                     `xdotool key` presses and releases inside one frame and
#                     the guest never sees it -- but a long hold is just as
#                     wrong: the guest's own pad object reports delay=480ms
#                     interval=96ms, so a 0.6-0.8s hold fires the initial press
#                     AND several auto-repeats. On a two-option dialog that
#                     moves the highlight there and back and nets zero visible
#                     change, which reads exactly like a dead button. Several
#                     Right presses measured 1.0 band delta for this reason
#                     while others measured 129. At 0.1s each press produces
#                     exactly one edge, and Right measures 134.
#   --capture-at SEC  comma-separated capture times; default is every 15s
#   --display :NN     X display to use (default :77)
#   --binary PATH     ac6recomp to run (default build-rt/ac6recomp)
#   --screen WxH      Xvfb screen size (default 1280x720)
#   --clock MODE      "wall" (default) or "present". With "present" the numbers
#                     in --keys and --capture-at are GUEST FRAMES, counted from
#                     the runtime's own per-frame PRESENT lines, instead of
#                     seconds. Use this whenever the build's speed is not the
#                     stock 60 FPS -- an instrumented build runs at 9-32 FPS, so
#                     a wall-clock press aimed at the dialog lands in the
#                     cutscene instead and the run looks like the guest ignored
#                     it.
#
# Two things about this setup are load-bearing and neither is obvious.
#
# The screen is 1280x720 because that is the game's output size, and captures
# are of the X root window. At that size root coordinates and game coordinates
# coincide, so the button band at y 590-660 is the band tools/ac6-band.py
# measures. On a larger root the game occupies a corner, the band lands on
# empty desktop, and every reading is of nothing at all.
#
# The attract loop freezes without input (measured, cycle 335). Start must be
# pressed to leave the title, and until it is, no menu exists and no other key
# does anything -- a Left press into the title screen produces a null result
# that looks exactly like a broken confirm button. Reaching the YES/NO dialog
# takes Start and then A; see --keys in the examples below.
#
#   # reach the dialog, then test that navigation moves the highlight
#   tools/ac6-run.sh --out /tmp/nav --duration 120 \
#       --keys "35:Escape:0.6,42:space:0.6,60:Left:0.6" \
#       --capture-at "34,40,48,55,65"
#
# Everything is timeout-guarded: a hung capture must not stall the experiment.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

OUT=""; DUR=""; KEYS=""; CAPTURE_AT=""; DISP=":77"; SCREEN="1280x720"
BIN="$REPO/build-rt/ac6recomp"
CLOCK="wall"
WAITFOR=""
THENKEYS=""

while [ $# -gt 0 ]; do
  case "$1" in
    --out)        OUT="$2"; shift 2 ;;
    --duration)   DUR="$2"; shift 2 ;;
    --keys)       KEYS="$2"; shift 2 ;;
    --capture-at) CAPTURE_AT="$2"; shift 2 ;;
    --display)    DISP="$2"; shift 2 ;;
    --binary)     BIN="$2"; shift 2 ;;
    --screen)     SCREEN="$2"; shift 2 ;;
    --clock)      CLOCK="$2"; shift 2 ;;
    --wait-for)   WAITFOR="$2"; shift 2 ;;
    --then-keys)  THENKEYS="$2"; shift 2 ;;
    --)           shift; break ;;
    *)            echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

[ -n "$OUT" ] && [ -n "$DUR" ] || { echo "usage: $0 --out DIR --duration SEC" >&2; exit 2; }
[ -x "$BIN" ] || { echo "FATAL: no runnable binary at $BIN" >&2; exit 1; }

for tool in Xvfb xdotool import; do
  command -v "$tool" >/dev/null || { echo "FATAL: $tool not on PATH" >&2; exit 1; }
done

mkdir -p "$OUT"; rm -f "$OUT"/*.png

# Default capture schedule: every 15s, stopping short of the shutdown window.
if [ -z "$CAPTURE_AT" ]; then
  t=15
  while [ "$t" -lt "$DUR" ]; do CAPTURE_AT="${CAPTURE_AT:+$CAPTURE_AT,}$t"; t=$((t + 15)); done
fi

pkill -x ac6recomp 2>/dev/null
pkill -f "Xvfb $DISP" 2>/dev/null
sleep 1

nohup Xvfb "$DISP" -screen 0 "${SCREEN}x24" >/dev/null 2>&1 &
sleep 3
pgrep -f "Xvfb $DISP" >/dev/null || { echo "FATAL: Xvfb did not start on $DISP" >&2; exit 1; }

# --mnk_mode=true is required: the keyboard/mouse driver is OFF by default and
# reports DEVICE_NOT_CONNECTED, so the guest sees no pad at all and no key
# reaches it whatever the guest does with it.
#
# Run from the binary's own directory: the runtime writes ac6recomp.log into the
# working directory, and keeping it next to the binary is what every existing
# script and every archived result assumes.
bindir="$(cd "$(dirname "$BIN")" && pwd)"
cd "$bindir" || exit 1
DISPLAY="$DISP" nohup timeout -k 5 "$DUR" "$BIN" \
    --mnk_mode=true --ac6_performance_mode=false --log_flush_interval=1 \
    "$@" > "$OUT/run.log" 2>&1 &
game_pid=$!

# Focus is taken immediately before every key, not once at startup.
#
# Focusing at t=5s and trusting it for the rest of the run silently loses every
# press: the window that exists during early boot is not necessarily the one
# receiving input later, and an unfocused MnK driver drops keys without
# reporting anything. A whole run then reads as "the guest ignored the input"
# when the input never arrived -- which is the single most expensive way to be
# wrong here, because it is indistinguishable from the defect under
# investigation.
focus_game() {
  local win
  win="$(DISPLAY=$DISP xdotool search --class ac6recomp 2>/dev/null | head -1)"
  if [ -n "$win" ]; then
    DISPLAY=$DISP xdotool windowactivate "$win" 2>/dev/null
    DISPLAY=$DISP xdotool windowfocus "$win" 2>/dev/null
    return 0
  fi
  return 1
}

# Merge the key events and the capture times into one ordered timeline, so a
# capture scheduled just after a press actually lands after it.
timeline="$(
  { IFS=','; for k in $KEYS;       do [ -n "$k" ] && echo "${k%%:*} key $k"; done
    IFS=','; for c in $CAPTURE_AT; do [ -n "$c" ] && echo "$c cap $c"; done
  } | sort -n -k1,1
)"

# Block until the runtime log says the guest has reached a given state.
#
# This is the reliable form of "pace the input on the executable, not the host".
# --clock present counts PRESENT lines, but those are emitted by the swap path
# and are not one per frame on every screen: a capture taken mid-run showed the
# overlay reporting a healthy 60.56 fps while the PRESENT count had almost
# stopped, so a frame-count schedule stalls for reasons that have nothing to do
# with the game's progress. Waiting for a state the guest itself announces has
# no such failure mode.
#
# --wait-for "\[ac6-screen-id\]" holds the timeline until the dialog screen
# actually exists, which is what every experiment on that dialog needs.
wait_for_log() {
  local pattern="$1" limit="$2" waited=0
  echo "  waiting for /$pattern/ in the runtime log (up to ${limit}s)"
  while ! grep -q "$pattern" "$bindir/ac6recomp.log" 2>/dev/null; do
    sleep 2
    waited=$((waited + 2))
    pgrep -x ac6recomp >/dev/null || { echo "  (game exited while waiting)"; return 1; }
    [ "$waited" -ge "$limit" ] && { echo "  (gave up waiting for /$pattern/)"; return 1; }
  done
  echo "  matched /$pattern/ after ${waited}s"
  return 0
}

# Advance the timeline on the guest's clock, not the host's.
#
# A wall-clock schedule assumes the game reaches a given screen at a given
# second. It does not: an instrumented build runs at 9-32 FPS against 60, and
# even two runs of the same binary vary, so a press aimed at the dialog lands in
# the cutscene and the run reports that the guest ignored it. Three captures
# were lost to this before the cause was clear.
#
# --clock present counts the runtime's own per-frame PRESENT lines instead, so
# positions in --keys and --capture-at are guest frames. The same schedule then
# lands on the same screen whatever the host speed.
guest_frames() {
  grep -c "PRESENT" "$bindir/ac6recomp.log" 2>/dev/null || echo 0
}

wait_until() {
  local target="$1" waited=0
  if [ "$CLOCK" = "present" ]; then
    while [ "$(guest_frames)" -lt "$target" ]; do
      sleep 1
      waited=$((waited + 1))
      # Never outlive the game: if it has exited, the frame count is frozen and
      # this would spin until the script's own timeout.
      pgrep -x ac6recomp >/dev/null || { echo "  (game gone, abandoning timeline)"; return 1; }
      [ "$waited" -gt "$DUR" ] && { echo "  (timeline gave up waiting for frame $target)"; return 1; }
    done
  else
    local delta
    delta=$(awk -v a="$target" -v n="$now" 'BEGIN{d=a-n; print (d>0)?d:0}')
    sleep "$delta"
  fi
  now="$target"
  return 0
}

clock_unit="s"; [ "$CLOCK" = "present" ] && clock_unit="f"

now=0
while read -r at kind payload; do
  [ -n "$at" ] || continue
  wait_until "$at" || break
  case "$kind" in
    key)
      key="$(echo "$payload" | cut -d: -f2)"
      hold="$(echo "$payload" | cut -d: -f3)"; hold="${hold:-0.5}"
      if focus_game; then
        DISPLAY=$DISP xdotool keydown "$key" 2>/dev/null
        sleep "$hold"
        DISPLAY=$DISP xdotool keyup "$key" 2>/dev/null
        echo "t=${at}${clock_unit}  held $key for ${hold}s"
      else
        echo "t=${at}${clock_unit}  SKIPPED $key -- no ac6recomp window to focus" >&2
      fi
      ;;
    cap)
      if DISPLAY=$DISP timeout 20 import -window root "$OUT/t${at}.png" 2>/dev/null; then
        echo "t=${at}${clock_unit}  captured  (game alive: $(pgrep -xc ac6recomp || echo 0))"
      else
        echo "t=${at}${clock_unit}  capture FAILED"
      fi
      ;;
  esac
done <<< "$timeline"

# The pre-sequence above only has to get the guest moving -- leave the title,
# skip the cutscene -- and its exact timing does not matter. The press the
# experiment is actually about does matter, and it must land on the screen under
# test, so it waits for the guest to announce that screen rather than for a
# clock. Every capture lost so far was a press that arrived before the dialog
# existed and was recorded as the guest ignoring it.
if [ -n "$WAITFOR" ]; then
  if wait_for_log "$WAITFOR" "$DUR"; then
    IFS=','; for k in $THENKEYS; do
      [ -n "$k" ] || continue
      key="$(echo "$k" | cut -d: -f2)"
      hold="$(echo "$k" | cut -d: -f3)"; hold="${hold:-0.6}"
      sleep "$(echo "$k" | cut -d: -f1)"
      if focus_game; then
        DISPLAY=$DISP xdotool keydown "$key" 2>/dev/null
        sleep "$hold"
        DISPLAY=$DISP xdotool keyup "$key" 2>/dev/null
        echo "  (post-wait) held $key for ${hold}s"
      else
        echo "  (post-wait) SKIPPED $key -- no window to focus" >&2
      fi
      DISPLAY=$DISP timeout 20 import -window root "$OUT/after-${key}.png" 2>/dev/null \
        && echo "  (post-wait) captured after-${key}.png"
    done
    unset IFS
  fi
fi

wait "$game_pid" 2>/dev/null
pkill -x ac6recomp 2>/dev/null
sleep 2
pkill -f "Xvfb $DISP" 2>/dev/null

# Keep the runtime log with the captures so a result directory is
# self-contained, and say so if it is missing rather than leaving a silently
# incomplete result.
if ! cp -f "$bindir/ac6recomp.log" "$OUT/ac6recomp.log" 2>/dev/null; then
  echo "WARNING: no runtime log at $bindir/ac6recomp.log" >&2
fi

echo "--- captures in $OUT ---"
ls -1 "$OUT"/*.png 2>/dev/null || echo "(none)"
