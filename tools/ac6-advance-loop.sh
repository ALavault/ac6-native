#!/usr/bin/env bash
# AC6 runtime advance loop.
#
# Each iteration: regenerate the corpus from the working config, rebuild the
# runtime, run it under gdb, find the REX_FATAL it aborts on, identify the
# [functions] entry strictly between branch target and source, remove it, and
# repeat.
#
# The vendored clone's config is never edited: WORKCFG is a copy. The clone's
# generated/ and build dir ARE written, and are restored from BASE_OUT by
# ac6-restore.sh.
#
# Stops when: the runtime no longer aborts in recompiled code, a trap has no
# candidate split (needs real work), or MAX iterations are done.
set -uo pipefail

REF=/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference
REX="$REF/thirdparty/rexglue-sdk/out/linux-amd64/rexglue"
LEAKS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/ac6-clean-runtime-leaks.sh"
WORK="${CLAUDE_JOB_DIR:-/tmp/ac6-$(id -u)}/tmp/ac6-loop"
WORKCFG="$WORK/ac6.toml"
LOG="$WORK/loop.log"
MAX="${1:-12}"

mkdir -p "$WORK/out"
log(){ echo "[$(date +%H:%M:%S)] $*" | tee -a "$LOG"; }

for i in $(seq 1 "$MAX"); do
  log "=== iteration $i ==="

  # 1. regenerate into the clone's generated/
  # codegen intermittently dies on a std::bad_alloc that is NOT reproducible
  # with the same config: observed 3/3 failures in a 45s window, then 8/8
  # successes on the identical input with 110 GB free and nothing competing.
  # Failures CLUSTER: one observed window failed 8/8 across 5 minutes with
  # 110 GB free and nothing competing, while the identical config passed
  # immediately before and after. Output-directory state is not the cause
  # (empty -> fail, 1 file -> pass, repeated). Retry across ~12 minutes.
  # Cycle 302 froze AC6 for a week by trusting a single occurrence.
  # Write each iteration into a FRESH output directory. Reusing one directory
  # correlates with the intermittent codegen abort: identical configs passed
  # 6/6 into fresh dirs while the reused dir failed repeatedly.
  ITEROUT="$WORK/out.$i"
  rm -rf "$ITEROUT"; mkdir -p "$ITEROUT"
  sed -i -E "s#^out_directory_path = .*#out_directory_path = \"$ITEROUT\"#" "$WORKCFG"
  rc=1
  for attempt in $(seq 1 16); do
    ( cd "$WORK" && timeout 300 "$REX" codegen "$WORKCFG" >"$WORK/codegen.$i.log" 2>&1 )
    rc=$?
    [ $rc -eq 0 ] && break
    log "codegen attempt $attempt failed rc=$rc (transient?), retrying"
    sleep 45
  done
  if [ $rc -ne 0 ]; then log "codegen FAILED rc=$rc after 16 attempts (~12 min window)"; exit 1; fi
  traps=$(grep -rho 'Unresolved branch from' "$ITEROUT"/*.cpp 2>/dev/null | wc -l)
  log "codegen ok; traps=$traps"

  cp -f "$ITEROUT"/*.cpp "$ITEROUT"/*.h "$ITEROUT/sources.cmake" "$REF/generated/" 2>/dev/null

  # 2. rebuild
  ( cd "$REF" && cmake --build out/build/linux-amd64-runtime-localdev -j12 >"$WORK/build.$i.log" 2>&1 )
  rc=$?
  if [ $rc -ne 0 ]; then log "BUILD FAILED rc=$rc"; tail -5 "$WORK/build.$i.log" | tee -a "$LOG"; exit 1; fi
  log "build ok"

  # 3. Decide whether it still aborts using a DIRECT run first. gdb on this
  # 168 MB LTO binary can spend minutes loading symbols, and an inconclusive
  # gdb log (no SIGABRT line, no backtrace) previously read as false success.
  #
  # `timeout N xvfb-run -a <exe>` does NOT bound the guest: timeout signals the
  # xvfb-run shell script, and ac6recomp is its grandchild, so the guest keeps
  # running with the loop's exit code taken from the wrapper. Cycle 323 found a
  # gdb from this loop still alive after 3 days with a zombie ac6recomp child,
  # and 28 orphaned 4.8 GB guest-memory reservations holding 9.8 GiB of tmpfs.
  # Bound the process group instead, and reap what the kill orphans.
  bounded_run() {  # bounded_run <seconds> <logfile> <command...>
    local secs="$1" logfile="$2"; shift 2
    ( cd "$REF" && setsid "$@" >"$logfile" 2>&1 ) &
    local wrapper=$! rc=0
    ( sleep "$secs"; kill -KILL -- "-$(ps -o pgid= -p "$wrapper" 2>/dev/null | tr -d ' ')" 2>/dev/null ) &
    local watchdog=$!
    wait "$wrapper"; rc=$?
    kill "$watchdog" 2>/dev/null
    pkill -KILL -f "linux-amd64-runtime-localdev/ac6recomp" 2>/dev/null
    pkill -KILL -u "$(id -u)" -f "Xvfb .*-auth /tmp/xvfb-run\." 2>/dev/null
    "$LEAKS" >>"$LOG" 2>&1
    return $rc
  }

  bounded_run 90 "$WORK/direct.$i.log" \
    xvfb-run -a ./out/build/linux-amd64-runtime-localdev/ac6recomp
  drc=$?
  if [ $drc -ne 134 ]; then
    log "runtime no longer aborts (direct exit=$drc). STOP -- verify manually."
    tail -15 "$WORK/direct.$i.log" | tee -a "$LOG"
    exit 0
  fi

  # It does abort: get the frame. Generous timeout for symbol loading, and the
  # log must actually contain a backtrace or we cannot trust it.
  bounded_run 600 "$WORK/gdb.$i.log" \
    xvfb-run -a gdb -batch -ex run -ex "bt 8" \
      --args ./out/build/linux-amd64-runtime-localdev/ac6recomp
  if ! grep -qE '^#[0-9]' "$WORK/gdb.$i.log"; then
    log "gdb produced no backtrace (inconclusive, not success). STOP."
    exit 4
  fi

  # 4. locate the failing REX_FATAL and the candidate split
  cand=$(python3 - "$WORK/gdb.$i.log" "$ITEROUT" "$WORKCFG" <<'PY'
import re,sys,pathlib
trace=pathlib.Path(sys.argv[1]).read_text(errors="replace")
gen=pathlib.Path(sys.argv[2]); cfg=pathlib.Path(sys.argv[3])
FR=re.compile(r'#\d+\s+0x[0-9a-f]+ in __imp__((?:rex_)?sub_[0-9A-F]+).*?\.cpp:(\d+)')
FA=re.compile(r'REX_FATAL\("Unresolved branch from 0x([0-9A-F]+) to 0x([0-9A-F]+)"\)')
m=FR.search(trace)
if not m: print("NOFRAME"); sys.exit()
fn,line=m.group(1),int(m.group(2))
src=tgt=None
for p in gen.glob("*.cpp"):
    L=p.read_text(errors="replace").splitlines()
    if 0<=line-1<len(L):
        f=FA.search(L[line-1])
        if f: src,tgt=int(f.group(1),16),int(f.group(2),16); break
if src is None: print("NOFATAL", fn, line); sys.exit()
addrs=sorted(int(a,16) for a in re.findall(r'^0x([0-9A-F]{8}) = \{', cfg.read_text(), re.M))
# If the target is itself a declared entry, THAT entry must go: otherwise the
# generated code emits `goto loc_<target>` with no such label and fails to
# compile. This takes priority over splits merely lying between the two.
if tgt in addrs:
    between=[tgt]
else:
    between=[a for a in addrs if min(src,tgt)<a<=max(src,tgt)]
print(f"{fn} {src:#010x} {tgt:#010x} " + (",".join(f"{a:08X}" for a in between) if between else "NOCAND"))
PY
)
  log "abort: $cand"
  set -- $cand
  fn="${1:-}"; pair="${2:-} -> ${3:-}"; splits="${4:-NOCAND}"
  if [ "$fn" = "NOFRAME" ] || [ "$fn" = "NOFATAL" ]; then
    log "abort is not an unresolved-branch trap. STOP for manual work."; exit 2
  fi
  if [ "$splits" = "NOCAND" ]; then
    log "trap $pair in $fn has NO candidate split. STOP for manual work."; exit 3
  fi

  # 5. remove the candidate splits from the working config
  python3 - "$WORKCFG" "$splits" <<'PY'
import sys,io,re
cfg,splits=sys.argv[1],sys.argv[2].split(",")
s=io.open(cfg,encoding="utf-8").read()
for a in splits:
    line=f'0x{a} = {{ name = "rex_sub_{a}" }}\n'
    if s.count(line)==1: s=s.replace(line,'',1); print(f"    removed 0x{a}")
    else: print(f"    WARN 0x{a} count={s.count(line)}")
io.open(cfg,'w',encoding='utf-8').write(s)
PY
  log "removed: $splits"
done
log "MAX iterations reached"
