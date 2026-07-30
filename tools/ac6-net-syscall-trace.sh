#!/usr/bin/env bash
# What does AC6 actually do to the socket its main thread dies on?
#
# ac6-main-thread-syscall.sh established that the main guest thread is parked
# forever in recvfrom(fd, buf, 1281, 0, ...) -- syscall 45, identical across
# every sample. The SDK contains exactly one recvfrom, XSocket::RecvFrom, and
# it is reachable only from the guest export NetDll_recvfrom. So the guest is
# doing this to itself through the network layer.
#
# Two readings are possible and they demand different fixes:
#
#   (a) the guest asked for a NON-BLOCKING socket and did not get one. On the
#       360 that is ioctlsocket(s, FIONBIO, &1), and FIONBIO there is the
#       Winsock value 0x8004667E. rex::net::socket_ioctl passes the guest's
#       command number straight to the Linux ioctl(), where FIONBIO is 0x5421,
#       so such a call cannot take effect and the socket stays blocking.
#
#   (b) the guest genuinely wants to block until a peer answers, in which case
#       no translation helps and the call must be answered or refused.
#
# Only the syscall sequence on that fd distinguishes them, so this traces it
# rather than arguing from the source. ioctl and fcntl are traced because they
# are how non-blocking is requested; they are post-filtered to the socket fds
# so the Vulkan driver's DRM traffic does not drown the result.
#
# Usage: ac6-net-syscall-trace.sh <label> [seconds]
set -uo pipefail

LABEL="${1:?usage: ac6-net-syscall-trace.sh <label> [seconds]}"
DURATION="${2:-90}"

ROOT=/fastdata/lavaulta/auto-re-agent
REF="$ROOT/.tools/ac6-recomp-reference"
EXE="$REF/out/build/linux-amd64-runtime-localdev/ac6recomp"
CLEANER="$ROOT/workspaces/ace-combat-6/tools/ac6-clean-runtime-leaks.sh"
OUT="${OUT_DIR:-$ROOT/workspaces/ace-combat-6/reports/logs/net-trace-$LABEL}"

mkdir -p "$OUT"
echo "== ac6-net-syscall-trace $LABEL =="
echo "sha256 : $(sha256sum "$EXE" | cut -d' ' -f1)"
echo "output : $OUT"

"$CLEANER" >"$OUT/leaks-before.txt" 2>&1

# -f to follow the guest threads; the socket may be created on one thread and
# received on another. %network covers socket/bind/connect/recvfrom/sendto/
# setsockopt/getsockopt/select; ioctl and fcntl are added for the FIONBIO path.
( cd "$REF" && strace -f -tt -s 64 \
    -e trace='%network,ioctl,fcntl' \
    -o "$OUT/strace.raw" \
    xvfb-run -a "$EXE" \
      --ac6_performance_mode=false \
      --log_flush_interval=1 \
      --frame_loop_telemetry_interval=300 >"$OUT/stdout.log" 2>&1 ) &
runner=$!

sleep "$DURATION"
pkill -x ac6recomp 2>/dev/null
sleep 3
pkill -x strace 2>/dev/null
wait "$runner" 2>/dev/null

{
  echo "== socket lifecycle: creation, binding, options =="
  grep -E 'socket\(|bind\(|connect\(|setsockopt\(|getsockopt\(|shutdown\(|close\(' \
    "$OUT/strace.raw" 2>/dev/null | grep -vE 'ENOENT|/dev/dri' | head -60

  echo
  echo "== every recvfrom / recvmsg =="
  grep -E 'recvfrom\(|recvmsg\(' "$OUT/strace.raw" 2>/dev/null | head -40

  echo
  echo "== every sendto / sendmsg =="
  grep -E 'sendto\(|sendmsg\(' "$OUT/strace.raw" 2>/dev/null | head -40

  echo
  echo "== select / poll on sockets =="
  grep -E '\bselect\(|pselect6\(' "$OUT/strace.raw" 2>/dev/null | head -20

  echo
  echo "== ioctl with a Winsock-looking command (0x8004667E FIONBIO etc.) =="
  grep -E 'ioctl\([0-9]+, (0x8|2147|-)' "$OUT/strace.raw" 2>/dev/null | head -30

  echo
  echo "== fcntl F_SETFL (the POSIX way to ask for non-blocking) =="
  grep -E 'fcntl\([0-9]+, F_SETFL' "$OUT/strace.raw" 2>/dev/null | head -20

  echo
  echo "== unfinished / blocked calls at the end of the trace =="
  grep -E 'unfinished' "$OUT/strace.raw" 2>/dev/null | tail -20

  echo
  echo "== counts =="
  for k in socket bind connect setsockopt recvfrom sendto select ioctl; do
    printf '%-12s %s\n' "$k" "$(grep -cE "\b$k\(" "$OUT/strace.raw" 2>/dev/null)"
  done
  echo "trace lines : $(wc -l <"$OUT/strace.raw" 2>/dev/null)"
} 2>&1 | tee "$OUT/verdict.txt"

"$CLEANER" >"$OUT/leaks-after.txt" 2>&1
echo
echo "written to $OUT"
