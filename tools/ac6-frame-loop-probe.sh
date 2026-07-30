#!/usr/bin/env bash
# Bounded AC6 runtime probe for the frame-loop frontier.
#
# Launches the runtime headless, samples the guest condition-variable state that
# cycles 320-323 localised, and records what the host actually presented. Every
# guest read goes through ac6_read_guest_memory.py, which qualifies the mapping
# against an anchor instruction before reporting anything, and declares the
# width and half of each value -- the cycle 322 lesson.
#
# Guest state sampled (module default.xex, SHA-256
# acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde):
#   0x82870818  condition object: +0 mutant handle, +4 event handle, +8 flag
#   0x82870828  shared 64-bit value; main thread wants 0, worker wants 1
#
# Usage: ac6-frame-loop-probe.sh <label> [seconds]
set -uo pipefail

LABEL="${1:?usage: ac6-frame-loop-probe.sh <label> [seconds]}"
DURATION="${2:-60}"

ROOT=/fastdata/lavaulta/auto-re-agent
REF="$ROOT/.tools/ac6-recomp-reference"
EXE="$REF/out/build/linux-amd64-runtime-localdev/ac6recomp"
READER="$ROOT/workspaces/ace-combat-6/scripts/ac6_read_guest_memory.py"
CLEANER="$ROOT/workspaces/ace-combat-6/tools/ac6-clean-runtime-leaks.sh"
OUT="${OUT_DIR:-$ROOT/workspaces/ace-combat-6/reports/logs/frame-loop-probe-$LABEL}"

mkdir -p "$OUT"
echo "== ac6-frame-loop-probe $LABEL =="
echo "executable  : $EXE"
echo "sha256      : $(sha256sum "$EXE" | cut -d' ' -f1)"
echo "duration    : ${DURATION}s"
echo "output      : $OUT"

"$CLEANER" >"$OUT/leaks-before.txt" 2>&1

# --ac6_performance_mode=false is MANDATORY for any measurement.
#
# It defaults to *true*, and ApplyAc6PerformanceModeOverrides then does
# REXCVAR_SET(log_level, "error"). The runtime therefore ships with its own
# diagnostics silenced: a default run emits exactly three [info] lines from
# Ac6recompAppCreate and nothing else for the rest of its life, and
# ac6recomp.log stays 0 bytes. With it false the same run logs 716 lines.
# This, not a missing patch, is why a bare run looks like "nothing happened".
#
# --log_flush_interval=1 matters because every probe ends in SIGKILL and the
# file sink is otherwise flushed only at shutdown.
#
# Do NOT pass --log_file: Ac6recompAppCreate unconditionally overrides it to
# "ac6recomp.log" in the runtime's own working directory, which is why the
# telemetry is copied out of $REF below rather than written straight to $OUT.
#
# 300 vblanks is about 5 s at the ~57 Hz the runtime actually paces at.
TELEMETRY_INTERVAL="${TELEMETRY_INTERVAL:-300}"
( cd "$REF" && xvfb-run -a "$EXE" \
    --ac6_performance_mode=false \
    --log_flush_interval=1 \
    --frame_loop_telemetry_interval="$TELEMETRY_INTERVAL" >"$OUT/stdout.log" 2>&1 ) &
runner=$!

# Wait for guest memory to exist; the reader refuses to guess before then.
pid=""
for _ in $(seq 1 60); do
  sleep 1
  pid=$(pgrep -f "linux-amd64-runtime-localdev/ac6recomp" | tail -1)
  [ -n "$pid" ] && grep -q "xenia_memory_" "/proc/$pid/maps" 2>/dev/null && break
  pid=""
done

if [ -z "$pid" ]; then
  echo "FAIL: guest memory never appeared"
  wait $runner
  "$CLEANER" >"$OUT/leaks-after.txt" 2>&1
  exit 1
fi
echo "guest pid   : $pid"

# `timeout` cannot bound this run: it would signal xvfb-run, the shell wrapper,
# and leave the ac6recomp grandchild alive. Bound the guest directly instead.
( sleep "$DURATION"; kill -KILL "$pid" 2>/dev/null ) &
watchdog=$!

sample=0
while kill -0 "$pid" 2>/dev/null; do
  sample=$((sample + 1))
  python3 "$READER" --pid "$pid" \
      --u64 0x82870828 --u32 0x82870828 --u32 0x8287082C \
      --dump 0x82870818:24 >"$OUT/guest-state.$sample.json" 2>&1 || true
  {
    echo "sample=$sample epoch=$(date +%s)"
    for task in /proc/$pid/task/*; do
      tid=$(basename "$task")
      name=$(tr -d '\0' <"$task/comm" 2>/dev/null)
      utime=$(awk '{print $14+$15}' "$task/stat" 2>/dev/null)
      wchan=$(tr -d '\0' <"$task/wchan" 2>/dev/null)
      [ -n "$name" ] && echo "  $tid $name ticks=$utime wchan=$wchan"
    done
  } >"$OUT/threads.$sample.txt" 2>/dev/null
  sleep 10
done

kill "$watchdog" 2>/dev/null
wait $runner
echo "runner exit : $?"
pkill -KILL -f "linux-amd64-runtime-localdev/ac6recomp" 2>/dev/null
# Killing the guest directly orphans the Xvfb that xvfb-run started for it, so
# reap it here: an abandoned Xvfb holds a display number and its Xauthority dir
# for as long as the box stays up.
pkill -KILL -u "$(id -u)" -f "Xvfb .*-auth /tmp/xvfb-run\." 2>/dev/null
find /tmp -maxdepth 1 -user "$(id -u)" -name 'xvfb-run.*' -type d -empty -delete 2>/dev/null
cp -f "$REF/ac6recomp.log" "$OUT/guest.log" 2>/dev/null || : >"$OUT/guest.log"

echo "-- guest condition value per sample (64-bit, big-endian) --"
for f in "$OUT"/guest-state.*.json; do
  python3 - "$f" <<'PY'
import json,sys,pathlib
p=pathlib.Path(sys.argv[1])
try:
    d=json.loads(p.read_text())
except Exception:
    print(f"  {p.name}: unreadable ({p.read_text()[:80].strip()})"); raise SystemExit
r=[x for x in d["reads"] if x.get("width_bits")==64]
o=[x for x in d["reads"] if x.get("length")==24]
val=r[0]["value"] if r else "?"
hi=r[0].get("high_half_u32") if r else "?"
lo=r[0].get("low_half_u32") if r else "?"
handles=[hex(v) for v in o[0]["u32_big_endian"][:3]] if o else []
print(f"  {p.name}: value64={val} high@0x82870828={hi} low@0x8287082C={lo} obj={handles}")
PY
done

echo "-- frame-loop telemetry (in-tree counters, gated on frame_loop_telemetry_interval) --"
if grep -h "\[frame-loop\]" "$OUT/guest.log" "$OUT/stdout.log" 2>/dev/null | tail -1 | grep -q .; then
  grep -h "\[frame-loop\]" "$OUT/guest.log" "$OUT/stdout.log" 2>/dev/null | head -1 | sed 's/^/  first: /'
  grep -h "\[frame-loop\]" "$OUT/guest.log" "$OUT/stdout.log" 2>/dev/null | tail -1 | sed 's/^/  last : /'
  printf '  lines: %s\n' "$(grep -hc "\[frame-loop\]" "$OUT/guest.log" "$OUT/stdout.log" 2>/dev/null | awk '{s+=$1} END{print s+0}')"
else
  echo "  NONE -- no telemetry line was emitted."
  echo "  Either the guest never reached a vblank interrupt with a registered handler,"
  echo "  or this binary predates the in-tree counters. Do not read this as 'zero frames'."
fi

echo "-- host output --"
count() { cat "$OUT/guest.log" "$OUT/stdout.log" 2>/dev/null | grep -c -- "$1"; }
printf 'guest log lines : %s\n' "$(wc -l <"$OUT/guest.log" 2>/dev/null || echo 0)"
printf 'stdout lines    : %s\n' "$(wc -l <"$OUT/stdout.log" 2>/dev/null || echo 0)"
printf 'XELOG_GPU PRESENT: %s\n' "$(count 'XELOG_GPU PRESENT')"
printf 'presentation skipped: %s\n' "$(count 'Skipping Vulkan frame presentation')"
printf 'DATA00.PAC read : %s\n' "$(count DATA00.PAC)"
printf 'REX_FATAL       : %s\n' "$(grep -c "REX_FATAL\|Unresolved" "$OUT/guest.log" "$OUT/stdout.log" 2>/dev/null | awk -F: '{s+=$2} END{print s+0}')"

"$CLEANER" >"$OUT/leaks-after.txt" 2>&1
cat "$OUT/leaks-after.txt"
