#!/usr/bin/env bash
# Run AC6 with the guest-socket interposer and report what the guest did.
#
# Two modes, and the order matters: observe first, then experiment.
#
#   observe     log every AF_INET socket call and dump a host backtrace at the
#               first blocking recvfrom. Changes no behaviour.
#   nonblock    additionally force AF_INET sockets non-blocking and translate a
#               Winsock FIONBIO into the POSIX one. This is the experiment: if
#               the guest's frame loop advances afterwards, the blocking
#               receive was the thing holding it.
#
# The ring counters are read out of the runtime's own log at the end so the
# experiment is judged on the P0 gate observables, not on "it looked further".
#
# Usage: ac6-net-probe.sh <observe|nonblock> <label> [seconds]
set -uo pipefail

MODE="${1:?usage: ac6-net-probe.sh <observe|nonblock> <label> [seconds]}"
LABEL="${2:?usage: ac6-net-probe.sh <observe|nonblock> <label> [seconds]}"
DURATION="${3:-60}"

ROOT=/fastdata/lavaulta/auto-re-agent
REF="$ROOT/.tools/ac6-recomp-reference"
EXE="$REF/out/build/linux-amd64-runtime-localdev/ac6recomp"
TOOLS="$ROOT/workspaces/ace-combat-6/tools"
CLEANER="$TOOLS/ac6-clean-runtime-leaks.sh"
OUT="${OUT_DIR:-$ROOT/workspaces/ace-combat-6/reports/logs/net-probe-$MODE-$LABEL}"

# Single-variable experiments: bindfix and nonblock are deliberately separable
# so a change in the guest's behaviour can be attributed to one of them.
case "$MODE" in
  observe)         NB=0; PO=0     ;;   # log only, no behaviour change
  bindfix)         NB=0; PO=40000 ;;   # make the privileged bind succeed
  nonblock)        NB=1; PO=0     ;;   # make the receive return instead of blocking
  bindfix-nonblock) NB=1; PO=40000 ;;  # both
  *) echo "mode must be observe|bindfix|nonblock|bindfix-nonblock"; exit 2 ;;
esac

mkdir -p "$OUT"
echo "== ac6-net-probe $MODE $LABEL =="
echo "sha256   : $(sha256sum "$EXE" | cut -d' ' -f1)"
echo "nonblock : $NB   port_offset: $PO   duration: ${DURATION}s"

"$CLEANER" >"$OUT/leaks-before.txt" 2>&1
rm -f "$OUT/net.log"

( cd "$REF" && \
  LD_PRELOAD="$TOOLS/ac6-net-interpose.so" \
  AC6_NET_LOG=1 AC6_NET_BT=1 AC6_NET_NONBLOCK="$NB" AC6_NET_PORT_OFFSET="$PO" \
  AC6_NET_LOGFILE="$OUT/net.log" \
  xvfb-run -a "$EXE" \
    --ac6_performance_mode=false \
    --log_flush_interval=1 \
    --frame_loop_telemetry_interval=300 >"$OUT/stdout.log" 2>&1 ) &
runner=$!

sleep "$DURATION"

# Snapshot the main guest thread's state before killing it: with the fix in
# place it should no longer be sitting in a receive.
pid="$(pgrep -x ac6recomp | head -1)"
if [ -n "$pid" ]; then
  for t in /proc/"$pid"/task/*; do
    case "$(tr -d '\n' <"$t/comm" 2>/dev/null)" in
      "Main XThread"*)
        {
          echo "main tid   : ${t##*/}"
          echo "wchan      : $(tr -d '\n' <"$t/wchan" 2>/dev/null)"
          echo "syscall    : $(cat "$t/syscall" 2>/dev/null)"
        } >"$OUT/main-thread.txt"
        ;;
    esac
  done
fi

cp -f "$REF/ac6recomp.log" "$OUT/ac6recomp.log" 2>/dev/null
[ -n "$pid" ] && kill -KILL "$pid" 2>/dev/null
sleep 2
pkill -KILL -x xvfb-run 2>/dev/null
kill -KILL "$runner" 2>/dev/null
cp -f "$REF/ac6recomp.log" "$OUT/ac6recomp.log" 2>/dev/null

{
  echo "== main guest thread at t+${DURATION}s =="
  cat "$OUT/main-thread.txt" 2>/dev/null || echo "(process already gone)"

  echo
  echo "== guest socket calls =="
  grep -vE '^\[ac6-net\] backtrace|^    #' "$OUT/net.log" 2>/dev/null | head -60

  echo
  echo "== backtrace at first guest recvfrom =="
  sed -n '/backtrace/,/^\[ac6-net\] recvfrom/p' "$OUT/net.log" 2>/dev/null | head -40

  echo
  echo "== P0 gate observables from the runtime log =="
  grep -oE '(eop|guest_swap_requests|host_swap_presents|wptr_updates|primary_executions)=[0-9]+' \
    "$OUT/ac6recomp.log" 2>/dev/null | tail -20
  echo "-- last frame-loop telemetry line --"
  grep 'frame-loop' "$OUT/ac6recomp.log" 2>/dev/null | tail -2 | cut -c1-400
  echo "-- guest log size --"
  wc -l "$OUT/ac6recomp.log" 2>/dev/null
} 2>&1 | tee "$OUT/verdict.txt"

"$CLEANER" >"$OUT/leaks-after.txt" 2>&1
echo
echo "written to $OUT"
