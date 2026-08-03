#!/usr/bin/env bash
# Send held key presses to an already-running ac6recomp on a given X display.
#
# Separate from ac6-run.sh because some experiments need to drive a game that is
# already up -- attaching to a long run, or reacting to something in the log --
# rather than following a schedule fixed before launch.
#
# Usage:
#   tools/ac6-input.sh [--display :77] [--hold 0.6] KEY [KEY ...]
#   tools/ac6-input.sh --list
#
# Keys are X keysyms. The bindings that matter, from the MnK driver defaults:
#
#   A       space     confirm         Start   Escape
#   B       shift     cancel          Back    BackSpace
#   X       r                         LB / RB q / f
#   Y       e                         L-stick w a s d
#   D-pad   Up Down Left Right
#
# Keys must be HELD. `xdotool key` presses and releases inside a single frame,
# and the guest's edge detector polls at frame rate, so a bare key press is not
# reliably observed at all -- which is how cycle 314 concluded input was dead
# when it was merely too fast. The default hold of 0.6 s spans the auto-repeat
# delay of 480 ms measured in the guest's own pad object.
set -uo pipefail

DISP=":77"
HOLD="0.6"
GAP="0.4"

while [ $# -gt 0 ]; do
  case "$1" in
    --display) DISP="$2"; shift 2 ;;
    --hold)    HOLD="$2"; shift 2 ;;
    --gap)     GAP="$2"; shift 2 ;;
    --list)
      sed -n '/^#   A  /,/^#   D-pad/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,3\}//'
      exit 0 ;;
    -*) echo "unknown option: $1" >&2; exit 2 ;;
    *)  break ;;
  esac
done

[ $# -gt 0 ] || { echo "usage: $0 [--display :77] [--hold 0.6] KEY [KEY ...]" >&2; exit 2; }
command -v xdotool >/dev/null || { echo "FATAL: xdotool not on PATH" >&2; exit 1; }

if ! pgrep -x ac6recomp >/dev/null; then
  echo "WARNING: no ac6recomp process is running; keys will go nowhere" >&2
fi

win="$(DISPLAY=$DISP xdotool search --class ac6recomp 2>/dev/null | head -1)"
if [ -n "$win" ]; then
  DISPLAY=$DISP xdotool windowfocus "$win" 2>/dev/null
else
  echo "WARNING: no ac6recomp window on $DISP; the driver ignores unfocused input" >&2
fi

for key in "$@"; do
  DISPLAY=$DISP xdotool keydown "$key" 2>/dev/null || { echo "keydown $key FAILED" >&2; continue; }
  sleep "$HOLD"
  DISPLAY=$DISP xdotool keyup "$key" 2>/dev/null
  echo "held $key for ${HOLD}s"
  sleep "$GAP"
done
