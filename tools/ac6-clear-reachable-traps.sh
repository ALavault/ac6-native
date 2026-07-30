#!/usr/bin/env bash
# Clear reachable REX_FATAL traps one at a time, automatically.
#
# Cycle 331 wrongly concluded the reference corpus was unreproducible. It is:
# the reference corpus contains the very trap the rebuilt runtime hit, with an
# identical trap profile (Unresolved branch 2071, Unresolved call 365). The
# reference *binary* holds that trap too and simply never reaches it. The
# rebuilt one reaches it because the cycle 328 socket fix lets the guest get
# further. Hitting a new trap is therefore progress, not regression -- it is the
# cycle 306-307 workflow, where each declared boundary exposes the next.
#
# Each round: run the runtime, read the one FATAL it died on, declare the target
# address in [functions], regenerate, rebuild, repeat. A branch target declared
# as a function becomes a tail call, which preserves context because rexglue
# passes ctx explicitly -- the cycle 312 pattern.
#
# Stops when a round produces no FATAL (the runtime survived the probe window),
# when the same address repeats (declaring it did not help), or at max rounds.
#
# Usage: ac6-clear-reachable-traps.sh [max_rounds] [probe_seconds]
set -uo pipefail

MAX="${1:-6}"
PROBE="${2:-70}"

WT=/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill
TOOLS=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/tools
OUT=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/reports/logs/clear-traps
mkdir -p "$OUT"
cd "$WT" || exit 1

declared=""
for round in $(seq 1 "$MAX"); do
  echo "===== round $round ====="

  "$TOOLS/ac6-clean-runtime-leaks.sh" >/dev/null 2>&1
  rm -f ac6recomp.log
  ( cd "$WT/build-rt" && xvfb-run -a ./ac6recomp \
      --ac6_performance_mode=false --log_flush_interval=1 \
      --frame_loop_telemetry_interval=300 >"$OUT/stdout-$round.log" 2>&1 ) &
  runner=$!
  sleep "$PROBE"
  pid="$(pgrep -x ac6recomp | head -1)"
  alive="no"; [ -n "$pid" ] && alive="yes"
  cp -f "$WT/build-rt/ac6recomp.log" "$OUT/round-$round.log" 2>/dev/null || \
    cp -f "$WT/ac6recomp.log" "$OUT/round-$round.log" 2>/dev/null
  [ -n "$pid" ] && kill -KILL "$pid" 2>/dev/null
  sleep 2; pkill -KILL -x xvfb-run 2>/dev/null; kill -KILL "$runner" 2>/dev/null

  echo "runtime alive after ${PROBE}s: $alive"
  # The P0 observables, whatever happened.
  grep -o 'eop=[0-9]* .*pm4_swap=[0-9]*' "$OUT/round-$round.log" 2>/dev/null | tail -1

  fatal="$(grep -o 'Unresolved \(branch\|call\) from 0x[0-9A-Fa-f]* to 0x[0-9A-Fa-f]*' \
           "$OUT/round-$round.log" 2>/dev/null | tail -1)"
  if [ -z "$fatal" ]; then
    echo "no FATAL this round -- stopping"
    break
  fi
  echo "FATAL: $fatal"
  target="$(echo "$fatal" | grep -o 'to 0x[0-9A-Fa-f]*' | sed 's/to //')"
  case " $declared " in
    *" $target "*) echo "already declared $target and it recurred -- stopping"; break ;;
  esac
  declared="$declared $target"

  python3 - "$target" <<'PYEOF'
import re, sys
addr = int(sys.argv[1], 16)
p = 'ac6recomp_config.toml'
lines = open(p).read().splitlines(True)
start = next(i for i, l in enumerate(lines) if l.strip() == '[functions]')
end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith('[')), len(lines))
pat = re.compile(r'^0x([0-9A-Fa-f]{8}) = ')
for i in range(start + 1, end):
    m = pat.match(lines[i])
    if m and int(m.group(1), 16) == addr:
        print(f'  0x{addr:08X} already declared'); sys.exit(0)
idx = next((i for i in range(start + 1, end)
            if pat.match(lines[i]) and int(pat.match(lines[i]).group(1), 16) > addr), end)
lines.insert(idx, f'0x{addr:08X} = {{ name = "rex_sub_{addr:08X}" }}\n')
open(p, 'w').writelines(lines)
print(f'  declared 0x{addr:08X} at line {idx + 1}')
PYEOF

  rm -rf generated && mkdir -p generated
  if ! timeout 900 ./thirdparty/rexglue-sdk/out/linux-amd64/rexglue codegen \
        ac6recomp_config.toml >"$OUT/codegen-$round.log" 2>&1; then
    echo "codegen FAILED -- stopping"; tail -3 "$OUT/codegen-$round.log"; break
  fi
  ( cd build-rt && timeout 1500 ninja ac6recomp >"$OUT/build-$round.log" 2>&1 ) || {
    echo "build FAILED -- stopping"; grep -E 'error:|FAILED' "$OUT/build-$round.log" | head -5; break; }
  echo "  regenerated and rebuilt"
done

echo "===== declared this session: $declared ====="
