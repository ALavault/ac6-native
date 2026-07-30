#!/usr/bin/env bash
# What loop is the AC6 main guest thread running?
#
# Cycle 328 moved the main thread from 0.0% CPU parked in a socket receive to
# ~121% CPU running guest code. That changes what can be measured: while the
# thread slept there were no samples to take, which is why cycle 326's profile
# said nothing about it. Now there are.
#
# The recompiled corpus emits one C++ function per guest function, named after
# the guest address (rex_sub_XXXXXXXX / sub_XXXXXXXX), so a symbol-level profile
# of this thread names guest functions directly -- no address arithmetic, no
# guest unwinder.
#
# Restricted to the single thread with -t: a whole-process DWARF record on this
# 165 MB LTO binary produces gigabytes and mostly captures the audio worker,
# which cycle 326 already showed burns a core on its own and is not the guest.
#
# Usage: ac6-main-thread-profile.sh <label> [settle_seconds] [record_seconds]
set -uo pipefail

LABEL="${1:?usage: ac6-main-thread-profile.sh <label> [settle] [record]}"
SETTLE="${2:-40}"
RECORD="${3:-20}"

ROOT=/fastdata/lavaulta/auto-re-agent
REF="$ROOT/.tools/ac6-recomp-reference"
EXE="$REF/out/build/linux-amd64-runtime-localdev/ac6recomp"
TOOLS="$ROOT/workspaces/ace-combat-6/tools"
CLEANER="$TOOLS/ac6-clean-runtime-leaks.sh"
OUT="${OUT_DIR:-$ROOT/workspaces/ace-combat-6/reports/logs/main-profile-$LABEL}"

mkdir -p "$OUT"
echo "== ac6-main-thread-profile $LABEL =="
echo "sha256 : $(sha256sum "$EXE" | cut -d' ' -f1)"
echo "settle : ${SETTLE}s   record: ${RECORD}s"

"$CLEANER" >"$OUT/leaks-before.txt" 2>&1

# The cycle 328 socket fix is applied through the interposer so this profile is
# taken on the post-fix guest, which is the one that actually runs. Exported
# inline: wrapping xvfb-run in `env` makes it exit immediately, measured.
export LD_PRELOAD="$TOOLS/ac6-net-interpose.so"
export AC6_NET_LOG=1 AC6_NET_NONBLOCK=0 AC6_NET_PORT_OFFSET=40000
export AC6_NET_LOGFILE="$OUT/net.log"

( cd "$REF" && xvfb-run -a "$EXE" \
    --ac6_performance_mode=false \
    --log_flush_interval=1 \
    --frame_loop_telemetry_interval=300 >"$OUT/stdout.log" 2>&1 ) &
runner=$!

pid=""
for _ in $(seq 1 40); do
  sleep 1
  pid="$(pgrep -x ac6recomp | head -1)"
  [ -n "$pid" ] && break
done
if [ -z "$pid" ]; then
  echo "FAIL: ac6recomp never appeared" | tee "$OUT/verdict.txt"; exit 1
fi
echo "pid    : $pid"
sleep "$SETTLE"

main_tid=""
for t in /proc/"$pid"/task/*; do
  case "$(tr -d '\n' <"$t/comm" 2>/dev/null)" in
    "Main XThread"*) main_tid="${t##*/}"; break ;;
  esac
done
if [ -z "$main_tid" ]; then
  echo "FAIL: no Main XThread" | tee "$OUT/verdict.txt"
  kill -KILL "$pid" 2>/dev/null; exit 1
fi
echo "main   : tid $main_tid  state=$(sed 's/.*) //' /proc/$pid/task/$main_tid/stat | cut -d' ' -f1)"

# Flat first: cheap, and enough to name the loop if it is one hot function.
perf record -F 499 -t "$main_tid" -o "$OUT/flat.data" -- sleep "$RECORD" \
  >"$OUT/record-flat.log" 2>&1
# Then with call graphs, to place that function in the guest's call chain.
perf record -F 199 --call-graph dwarf,16384 -t "$main_tid" -o "$OUT/graph.data" \
  -- sleep "$RECORD" >"$OUT/record-graph.log" 2>&1

kill -KILL "$pid" 2>/dev/null
sleep 1
pkill -KILL -x xvfb-run 2>/dev/null
kill -KILL "$runner" 2>/dev/null

{
  echo "== flat profile, main guest thread =="
  perf report -i "$OUT/flat.data" --stdio --no-children -F overhead,symbol 2>/dev/null | \
    grep -vE '^#|^$' | head -30

  echo
  echo "== call graph, heaviest chains =="
  perf report -i "$OUT/graph.data" --stdio --children -F overhead,symbol 2>/dev/null | \
    grep -vE '^#|^$' | head -40

  echo
  echo "== guest functions seen (sub_/rex_sub_ symbols) =="
  perf script -i "$OUT/flat.data" 2>/dev/null | \
    grep -oE '\b(rex_)?sub_[0-9A-Fa-f]{8}\b' | sort | uniq -c | sort -rn | head -25
} 2>&1 | tee "$OUT/verdict.txt"

"$CLEANER" >"$OUT/leaks-after.txt" 2>&1
echo
echo "written to $OUT"
