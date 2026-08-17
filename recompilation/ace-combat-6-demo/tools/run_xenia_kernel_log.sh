#!/usr/bin/env bash
# Record the oracle's kernel/XAM call sequence for the qualified demo XEX.
#
# Xenia is an oracle only.  This captures WHICH kernel exports the retail
# runtime services and in WHAT ORDER, to diff against the native runtime's
# AC6_DEMO_WATCH_IMPORTS journal.  Nothing it produces -- pixels, EDRAM,
# shaders, timings -- is admissible as native product behaviour.
#
# Xenia logs kernel exports at debug level ('d') in PrintKernelCall, and hides
# the high-frequency ones (NtSetEvent, XamInputGetState, RtlEnterCriticalSection
# ...) behind their own flag.  It never logs return values.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
launcher="$repository_root/scripts/run_xenia_edge_native.sh"
xex="${AC6_DEMO_XEX:-$repository_root/demo-game-file/extracted/stfs-root/Default.xex}"
xex_sha256="de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
seconds="${AC6_XENIA_LOG_SECONDS:-60}"
output="${1:-}"

if [[ -z "$output" ]]; then
  echo "usage: run_xenia_kernel_log.sh OUTPUT.log" >&2
  exit 2
fi
[[ -x "$launcher" ]] || { echo "launcher absent: $launcher" >&2; exit 2; }
[[ -f "$xex" ]] || { echo "demo XEX absent: $xex" >&2; exit 2; }
printf '%s  %s\n' "$xex_sha256" "$xex" | sha256sum -c - >/dev/null

# --gpu=null keeps this headless; the run is a kernel-call capture, never a
# rendering one.  A build without a null GPU falls back to vulkan under Xvfb.
timeout --signal=INT --kill-after=10s "${seconds}s" \
  "$launcher" \
    --headless=true \
    --gpu=null \
    --apu=nop \
    --log_level=3 \
    --log_high_frequency_kernel_calls=true \
    --log_to_stdout=false \
    "--log_file=$output" \
    "$xex" || true

[[ -s "$output" ]] || { echo "no oracle log produced: $output" >&2; exit 1; }
printf 'xenia_kernel_log=ok seconds=%s lines=%s path=%s\n' \
  "$seconds" "$(wc -l <"$output")" "$output"
