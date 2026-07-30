#!/usr/bin/env bash
# Where is the AC6 main guest thread?
#
# Cycle 327 closed P0.2 with a guest wait census built inside the three Nt*
# wait exports, and its central result was negative: the main thread appears in
# none of them. Cycle 326 had separately measured "Main XThread" at 1.7% of CPU
# -- small, but not zero, so the thread is not simply parked on an object.
#
# This probe answers "where is it?" from outside the runtime, with no rebuild
# and no instrumentation, by reading what the kernel already knows about every
# host thread of the process:
#
#   comm    the thread name, which is how a guest XThread is identifiable
#   stat    field 3 is the state: R running, S interruptible, D uninterruptible
#   wchan   the kernel function the thread is blocked in, empty when running
#   utime   +stime, in clock ticks, sampled twice to give a real CPU rate
#
# A thread that is genuinely parked shows S, a stable wchan (futex_wait and
# friends), and a flat CPU delta. A thread that is polling shows an oscillating
# state and a non-zero delta. The two are indistinguishable in a wait census
# that only records object waits -- which is exactly why the main thread was
# invisible at cycle 327.
#
# Usage: ac6-thread-state-census.sh <label> [settle_seconds] [sample_seconds]
set -uo pipefail

LABEL="${1:?usage: ac6-thread-state-census.sh <label> [settle] [sample]}"
SETTLE="${2:-30}"
SAMPLE="${3:-10}"

ROOT=/fastdata/lavaulta/auto-re-agent
REF="$ROOT/.tools/ac6-recomp-reference"
EXE="$REF/out/build/linux-amd64-runtime-localdev/ac6recomp"
CLEANER="$ROOT/workspaces/ace-combat-6/tools/ac6-clean-runtime-leaks.sh"
OUT="${OUT_DIR:-$ROOT/workspaces/ace-combat-6/reports/logs/thread-census-$LABEL}"

mkdir -p "$OUT"
echo "== ac6-thread-state-census $LABEL =="
echo "executable : $EXE"
echo "sha256     : $(sha256sum "$EXE" | cut -d' ' -f1)"
echo "settle     : ${SETTLE}s   sample: ${SAMPLE}s"
echo "output     : $OUT"

"$CLEANER" >"$OUT/leaks-before.txt" 2>&1

# Same mandatory flags as ac6-frame-loop-probe.sh: performance mode silences
# the runtime's own log, and the flush interval matters because we end in KILL.
# Optional interposer, so the census can be taken with a candidate fix active
# and the new frontier compared against the old one on identical instruments.
# These are exported rather than passed via `env`: wrapping xvfb-run in `env`
# makes it exit immediately with status 0, measured, so inline export it is.
if [ -n "${AC6_PRELOAD:-}" ]; then
  export LD_PRELOAD="$AC6_PRELOAD"
  export AC6_NET_LOG="${AC6_NET_LOG:-1}"
  export AC6_NET_NONBLOCK="${AC6_NET_NONBLOCK:-0}"
  export AC6_NET_PORT_OFFSET="${AC6_NET_PORT_OFFSET:-0}"
  export AC6_NET_LOGFILE="$OUT/net.log"
  echo "preload    : $AC6_PRELOAD (port_offset=$AC6_NET_PORT_OFFSET)"
fi

( cd "$REF" && xvfb-run -a "$EXE" \
    --ac6_performance_mode=false \
    --log_flush_interval=1 \
    --frame_loop_telemetry_interval=300 >"$OUT/stdout.log" 2>&1 ) &
runner=$!

# xvfb-run forks; the runtime is a descendant, so resolve it by name.
pid=""
for _ in $(seq 1 40); do
  sleep 1
  pid="$(pgrep -x ac6recomp | head -1)"
  [ -n "$pid" ] && break
done
if [ -z "$pid" ]; then
  echo "FAIL: ac6recomp never appeared" | tee "$OUT/verdict.txt"
  kill "$runner" 2>/dev/null
  exit 1
fi
echo "pid        : $pid"

echo "settling ${SETTLE}s so the frame loop reaches its frozen state..."
sleep "$SETTLE"

snapshot() {
  local tag="$1"
  : >"$OUT/threads-$tag.txt"
  for t in /proc/"$pid"/task/*; do
    tid="${t##*/}"
    [ -r "$t/stat" ] || continue
    comm="$(tr -d '\n' <"$t/comm" 2>/dev/null)"
    # Fields after the parenthesised comm: state is the first of them.
    rest="$(sed 's/.*) //' "$t/stat" 2>/dev/null)"
    state="$(echo "$rest" | cut -d' ' -f1)"
    utime="$(echo "$rest" | cut -d' ' -f12)"
    stime="$(echo "$rest" | cut -d' ' -f13)"
    wchan="$(tr -d '\n' <"$t/wchan" 2>/dev/null)"
    printf '%s\t%s\t%s\t%s\t%s\n' "$tid" "$state" "$((utime + stime))" "${wchan:--}" "$comm" \
      >>"$OUT/threads-$tag.txt"
  done
}

snapshot a
sleep "$SAMPLE"
snapshot b

# Join the two snapshots on tid and report the CPU delta, busiest first.
awk -v secs="$SAMPLE" -F'\t' '
  NR==FNR { cpu0[$1]=$3; next }
  {
    d = $3 - (($1 in cpu0) ? cpu0[$1] : $3);
    # 100 clock ticks per second on this kernel.
    printf "%s\t%s\t%6.1f%%\t%s\t%s\n", $1, $2, d*100.0/(100.0*secs), $4, $5;
  }
' "$OUT/threads-a.txt" "$OUT/threads-b.txt" \
  | sort -t$'\t' -k3 -rn >"$OUT/thread-cpu.txt"

{
  echo "tid    st  cpu%    wchan                     comm"
  awk -F'\t' '{printf "%-6s %-3s %-7s %-25s %s\n", $1, $2, $3, $4, $5}' "$OUT/thread-cpu.txt"
} | tee "$OUT/verdict.txt"

echo
echo "-- main guest thread rows --"
grep -iE 'Main XThread' "$OUT/verdict.txt" || echo "(no thread named 'Main XThread')"

kill -KILL "$pid" 2>/dev/null
wait "$runner" 2>/dev/null
"$CLEANER" >"$OUT/leaks-after.txt" 2>&1
echo
echo "written to $OUT"
