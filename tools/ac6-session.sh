#!/usr/bin/env bash
# Launch ac6recomp on a headless X display and expose it over VNC, so a human
# can drive it directly.
#
# Why this exists: scripted input has to predict when the game reaches a screen,
# and it is bad at that. The cutscene after the legal notices varies in length,
# an instrumented build runs at a third of the frame rate, and a press aimed at
# a dialog that has not appeared yet produces a null result indistinguishable
# from the defect being investigated. A person watching the screen has none of
# those problems, and can follow the actual path a player takes -- new game,
# through the menus, to the first mission -- which no key schedule here has
# managed.
#
# Pair it with tools/ac6-run.sh --clock present for the automated captures; this
# script is for the parts that need eyes.
#
# Usage:
#   tools/ac6-session.sh [--binary PATH] [--display :78] [--port 5900]
#                        [--password PASS] [--] [extra ac6recomp args]
#
# Connecting, when the machine running this is remote:
#
#   ssh -L 5900:localhost:5900 <user>@<this-host>
#   # then point any VNC client at localhost:5900
#
# The tunnel is the recommended route: without --password x11vnc is left open,
# and forwarding it over ssh keeps it off the network. With --password the VNC
# password is set from the value given.
#
# Anything the runtime logs goes to the usual ac6recomp.log next to the binary,
# so a session driven by hand produces the same evidence a scripted run does --
# including trace captures, if the instrumented binary is used with
# --ac6_trace_guest_calls=true.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BIN="$REPO/build-rt/ac6recomp"
DISP=":78"
PORT="5900"
PASSWORD=""
SCREEN="1280x720"

while [ $# -gt 0 ]; do
  case "$1" in
    --binary)   BIN="$2"; shift 2 ;;
    --display)  DISP="$2"; shift 2 ;;
    --port)     PORT="$2"; shift 2 ;;
    --password) PASSWORD="$2"; shift 2 ;;
    --screen)   SCREEN="$2"; shift 2 ;;
    --)         shift; break ;;
    *)          echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

[ -x "$BIN" ] || { echo "FATAL: no runnable binary at $BIN" >&2; exit 1; }
for tool in Xvfb x11vnc; do
  command -v "$tool" >/dev/null || { echo "FATAL: $tool not on PATH" >&2; exit 1; }
done

cleanup() {
  echo
  echo "shutting the session down"
  pkill -x ac6recomp 2>/dev/null
  pkill -f "x11vnc.*$DISP" 2>/dev/null
  pkill -f "Xvfb $DISP" 2>/dev/null
}
trap cleanup EXIT INT TERM

pkill -x ac6recomp 2>/dev/null
pkill -f "x11vnc.*$DISP" 2>/dev/null
pkill -f "Xvfb $DISP" 2>/dev/null
sleep 1

nohup Xvfb "$DISP" -screen 0 "${SCREEN}x24" >/dev/null 2>&1 &
sleep 3
pgrep -f "Xvfb $DISP" >/dev/null || { echo "FATAL: Xvfb did not start on $DISP" >&2; exit 1; }

vnc_args=(-display "$DISP" -rfbport "$PORT" -forever -shared -nopw -bg -quiet)
if [ -n "$PASSWORD" ]; then
  vnc_args=(-display "$DISP" -rfbport "$PORT" -forever -shared -passwd "$PASSWORD" -bg -quiet)
fi
x11vnc "${vnc_args[@]}" >/dev/null 2>&1
sleep 1
pgrep -f "x11vnc.*$DISP" >/dev/null || { echo "FATAL: x11vnc did not start" >&2; exit 1; }

bindir="$(cd "$(dirname "$BIN")" && pwd)"
cd "$bindir" || exit 1

echo "=============================================================="
echo " AC6 interactive session"
echo "   binary   : $BIN"
echo "   display  : $DISP  (${SCREEN})"
echo "   VNC      : port $PORT${PASSWORD:+ (password set)}"
echo
echo " From your machine:"
echo "   ssh -L ${PORT}:localhost:${PORT} $(whoami)@$(hostname)"
echo "   then connect a VNC client to localhost:${PORT}"
echo
echo " Controls (MnK driver defaults):"
echo "   A = space    B = shift    X = r    Y = e"
echo "   Start = Escape           Back = BackSpace"
echo "   D-pad = arrow keys       L-stick = w a s d"
echo
echo " The attract loop freezes without input: press Escape (Start) first."
echo " The cutscene after the legal notices can be skipped."
echo
echo " Log: $bindir/ac6recomp.log"
echo " Ctrl-C here ends the session."
echo "=============================================================="
echo

# --mnk_mode=true is required: the keyboard/mouse driver is off by default and
# reports DEVICE_NOT_CONNECTED, so the guest sees no pad at all.
DISPLAY="$DISP" "$BIN" \
    --mnk_mode=true --ac6_performance_mode=false --log_flush_interval=1 \
    "$@"
