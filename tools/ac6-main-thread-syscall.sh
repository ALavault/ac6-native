#!/usr/bin/env bash
# What exactly is the AC6 main guest thread blocked on?
#
# ac6-thread-state-census.sh established that the main guest thread sits at
# 0.0% CPU in the kernel function __skb_wait_for_more_packets -- a blocking
# datagram receive, not a futex and not a guest object wait. That is why the
# cycle 327 wait census, which instruments only the Nt* object-wait exports,
# could not see it.
#
# This probe names the socket. /proc/<pid>/task/<tid>/syscall gives the syscall
# number and its first six arguments; for a receive the first argument is the
# file descriptor, which /proc/<pid>/fd resolves to a socket inode, which
# /proc/net/{unix,udp,udp6,netlink} resolves to a peer. Sampled repeatedly so a
# steady block is distinguishable from a poll that happens to be caught inside.
#
# Usage: ac6-main-thread-syscall.sh <label> [settle_seconds] [samples]
set -uo pipefail

LABEL="${1:?usage: ac6-main-thread-syscall.sh <label> [settle] [samples]}"
SETTLE="${2:-30}"
SAMPLES="${3:-5}"

ROOT=/fastdata/lavaulta/auto-re-agent
REF="$ROOT/.tools/ac6-recomp-reference"
EXE="$REF/out/build/linux-amd64-runtime-localdev/ac6recomp"
CLEANER="$ROOT/workspaces/ace-combat-6/tools/ac6-clean-runtime-leaks.sh"
OUT="${OUT_DIR:-$ROOT/workspaces/ace-combat-6/reports/logs/main-thread-syscall-$LABEL}"

mkdir -p "$OUT"
echo "== ac6-main-thread-syscall $LABEL =="
echo "sha256 : $(sha256sum "$EXE" | cut -d' ' -f1)"

"$CLEANER" >"$OUT/leaks-before.txt" 2>&1

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
  echo "FAIL: ac6recomp never appeared" | tee "$OUT/verdict.txt"
  kill "$runner" 2>/dev/null; exit 1
fi
echo "pid    : $pid"
sleep "$SETTLE"

# Locate the main guest thread by name.
main_tid=""
for t in /proc/"$pid"/task/*; do
  case "$(tr -d '\n' <"$t/comm" 2>/dev/null)" in
    "Main XThread"*) main_tid="${t##*/}"; break ;;
  esac
done
if [ -z "$main_tid" ]; then
  echo "FAIL: no thread named 'Main XThread'" | tee "$OUT/verdict.txt"
  kill -KILL "$pid" 2>/dev/null; exit 1
fi
echo "main   : tid $main_tid"

{
  echo "== main guest thread, $SAMPLES samples =="
  for i in $(seq 1 "$SAMPLES"); do
    sc="$(cat /proc/"$pid"/task/"$main_tid"/syscall 2>/dev/null)"
    wc_="$(tr -d '\n' </proc/"$pid"/task/"$main_tid"/wchan 2>/dev/null)"
    st="$(sed 's/.*) //' /proc/"$pid"/task/"$main_tid"/stat 2>/dev/null | cut -d' ' -f1)"
    echo "[$i] state=$st wchan=$wc_"
    echo "    syscall: $sc"
    # First argument of the syscall is the fd for the recv family.
    fd="$(echo "$sc" | awk '{print $2}')"
    fd_dec=$((fd))
    link="$(readlink /proc/"$pid"/fd/"$fd_dec" 2>/dev/null)"
    echo "    fd=$fd_dec -> ${link:-<unresolved>}"
    sleep 2
  done

  echo
  echo "== socket inode resolution =="
  sc="$(cat /proc/"$pid"/task/"$main_tid"/syscall 2>/dev/null)"
  fd_dec=$(( $(echo "$sc" | awk '{print $2}') ))
  link="$(readlink /proc/"$pid"/fd/"$fd_dec" 2>/dev/null)"
  inode="$(echo "$link" | sed -n 's/^socket:\[\([0-9]*\)\]$/\1/p')"
  echo "fd $fd_dec link=$link inode=${inode:-none}"
  if [ -n "$inode" ]; then
    for f in /proc/net/unix /proc/net/udp /proc/net/udp6 /proc/net/netlink /proc/net/packet; do
      hit="$(awk -v ino="$inode" '$0 ~ ino' "$f" 2>/dev/null)"
      [ -n "$hit" ] && { echo "-- $f --"; echo "$hit"; }
    done
  fi

  echo
  echo "== all sockets held by the process =="
  for d in /proc/"$pid"/fd/*; do
    l="$(readlink "$d" 2>/dev/null)"
    case "$l" in socket:*) echo "  fd ${d##*/} -> $l" ;; esac
  done
} 2>&1 | tee "$OUT/verdict.txt"

kill -KILL "$pid" 2>/dev/null
wait "$runner" 2>/dev/null
"$CLEANER" >"$OUT/leaks-after.txt" 2>&1
echo
echo "written to $OUT"
