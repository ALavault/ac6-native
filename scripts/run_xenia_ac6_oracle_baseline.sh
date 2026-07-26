#!/usr/bin/env bash
# Read-only, reproducible Xenia baseline for the local AC6 PAL XEX.
#
# It is an oracle capture, not a native runtime and not evidence of scene
# parity.  Each invocation owns a fresh X server and Xenia home directory so
# screenshots and logs are attributable to exactly one retail launch.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
workspace_root="$repository_root/workspaces/ace-combat-6"
output_directory_input="${1:?usage: $0 OUTPUT_DIRECTORY [DISPLAY]}"
display_number="${2:-${XENIA_AC6_DISPLAY:-:177}}"
xenia_application="$repository_root/.tools/xenia-canary/build/bin/Linux/Release/xenia_canary"
xex_image="$workspace_root/game-files/default.xex"
# Keep the default under the bounded oracle runner's execution window.  A
# longer pair is permitted only with an explicit, observed log-progress gate;
# an unchanged black frame is not a reason to wait longer.
read -r -a capture_seconds <<< "${XENIA_AC6_CAPTURE_SECONDS:-8 20}"
ungated_max_seconds="${XENIA_AC6_UNGATED_MAX_SECONDS:-20}"
required_log_regex="${XENIA_AC6_REQUIRED_LOG_REGEX:-}"

# Xenia requires HOME/XDG paths to be absolute.  Resolve relative output paths
# against the repository root so the oracle's manifest and its isolated home
# describe the same directory regardless of the caller's current directory.
case "$output_directory_input" in
    /*) output_directory="$output_directory_input" ;;
    *) output_directory="$repository_root/$output_directory_input" ;;
esac

if [[ -e "$output_directory" ]]; then
    echo "refusing to overwrite existing output: $output_directory" >&2
    exit 2
fi
if [[ ! -x "$xenia_application" || ! -f "$xex_image" ]]; then
    echo "required local Xenia oracle input is unavailable" >&2
    exit 2
fi
if ! command -v Xvfb >/dev/null || ! command -v xdpyinfo >/dev/null || \
   ! command -v import >/dev/null; then
    echo "Xvfb, xdpyinfo, and ImageMagick import are required" >&2
    exit 2
fi
if [[ "${#capture_seconds[@]}" -ne 2 ]] || ! [[ "${capture_seconds[0]}" =~ ^[1-9][0-9]*$ ]] || \
   ! [[ "${capture_seconds[1]}" =~ ^[1-9][0-9]*$ ]] || \
   (( capture_seconds[1] <= capture_seconds[0] )); then
    echo "XENIA_AC6_CAPTURE_SECONDS must contain exactly two increasing positive seconds" >&2
    exit 2
fi
if ! [[ "$ungated_max_seconds" =~ ^[1-9][0-9]*$ ]]; then
    echo "XENIA_AC6_UNGATED_MAX_SECONDS must be a positive integer" >&2
    exit 2
fi
if (( capture_seconds[1] > ungated_max_seconds )) && [[ -z "$required_log_regex" ]]; then
    echo "captures beyond XENIA_AC6_UNGATED_MAX_SECONDS require XENIA_AC6_REQUIRED_LOG_REGEX" >&2
    exit 2
fi

# Never attach to an existing server: that invalidates both input/display
# attribution and the isolated Xenia profile contract.
display_lock="/tmp/.X${display_number#:}-lock"
if [[ -e "$display_lock" ]] || DISPLAY="$display_number" xdpyinfo >/dev/null 2>&1; then
    echo "requested X display is already occupied: $display_number" >&2
    exit 2
fi

mkdir -p "$output_directory" "$output_directory/home" "$output_directory/runtime"
chmod 700 "$output_directory/home" "$output_directory/runtime"
xvfb_pid=""
xenia_pid=""
progress_gate_status="not-required"
cleanup() {
    [[ -n "$xenia_pid" ]] && kill "$xenia_pid" 2>/dev/null || true
    [[ -n "$xvfb_pid" ]] && kill "$xvfb_pid" 2>/dev/null || true
    [[ -n "$xenia_pid" ]] && wait "$xenia_pid" 2>/dev/null || true
    [[ -n "$xvfb_pid" ]] && wait "$xvfb_pid" 2>/dev/null || true
}
trap cleanup EXIT

Xvfb "$display_number" -screen 0 1280x720x24 -nolisten tcp \
    >"$output_directory/xvfb.log" 2>&1 &
xvfb_pid=$!
for _ in $(seq 1 20); do
    DISPLAY="$display_number" xdpyinfo >/dev/null 2>&1 && break
    sleep 0.25
done
DISPLAY="$display_number" xdpyinfo >/dev/null 2>&1 || {
    echo "failed to start isolated Xvfb display: $display_number" >&2
    exit 3
}

HOME="$output_directory/home" \
XDG_CONFIG_HOME="$output_directory/home/.config" \
XDG_CACHE_HOME="$output_directory/home/.cache" \
XDG_RUNTIME_DIR="$output_directory/runtime" \
DISPLAY="$display_number" "$xenia_application" "$xex_image" \
    >"$output_directory/xenia.log" 2>&1 &
xenia_pid=$!

printf 'capture_seconds=%s\nungated_max_seconds=%s\nrequired_log_regex=%s\n' \
    "${capture_seconds[*]}" "$ungated_max_seconds" "$required_log_regex" \
    >"$output_directory/experiment-plan.txt"

wait_for_capture_second() {
    local delay_seconds=$1
    local capture_second=$2
    local elapsed=0
    while (( elapsed < delay_seconds )); do
        if ! kill -0 "$xenia_pid" 2>/dev/null; then
            printf 'status=xenia-exited-before-capture\nrequested_second=%s\n' \
                "$capture_second" >"$output_directory/terminal-status.txt"
            return 1
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
}

last_second=0
for capture_index in "${!capture_seconds[@]}"; do
    capture_second="${capture_seconds[$capture_index]}"
    capture_delay=$((capture_second - last_second))
    if ! wait_for_capture_second "$capture_delay" "$capture_second"; then
        exit 4
    fi
    if (( capture_index > 0 )) && [[ -n "$required_log_regex" ]]; then
        if ! grep -Eq -- "$required_log_regex" "$output_directory/xenia.log"; then
            progress_gate_status="failed"
            printf 'status=required-log-progress-not-observed\nregex=%s\ncheckpoint_second=%s\n' \
                "$required_log_regex" "$capture_second" \
                >"$output_directory/progress-gate.txt"
            exit 4
        fi
        progress_gate_status="passed"
    fi
    DISPLAY="$display_number" import -window root \
        "$output_directory/xenia-${capture_second}s.png" \
        2>>"$output_directory/capture.stderr" || true
    last_second="$capture_second"
done

sha256sum "$xenia_application" "$xex_image" "$output_directory"/*.png \
    >"$output_directory/sha256sums.txt"
printf 'status=completed\nprogress_gate=%s\n' "$progress_gate_status" \
    >"$output_directory/terminal-status.txt"
python3 - "$output_directory" "$display_number" "$xenia_application" "$xex_image" \
    "$progress_gate_status" "$required_log_regex" <<'PY'
import hashlib
import json
import pathlib
import sys

output, display, xenia, xex = map(pathlib.Path, (sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]))
progress_gate_status = sys.argv[5]
required_log_regex = sys.argv[6]
def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
captures = []
for path in sorted(output.glob("xenia-*.png")):
    captures.append({"file": path.name, "sha256": sha256(path)})
payload = {
    "schema": 1,
    "kind": "xenia-retail-launch-baseline",
    "oracle_only": True,
    "scene_parity_proved": False,
    "retail_xex": {"path": str(xex), "sha256": sha256(xex)},
    "xenia": {"path": str(xenia), "sha256": sha256(xenia)},
    "display": str(display),
    "captures": captures,
    "log": "xenia.log",
    "progress_gate": {
        "status": progress_gate_status,
        "required_log_regex": required_log_regex or None,
    },
    "limitations": [
        "No deterministic controller replay or retail profile is supplied.",
        "A capture proves only visible oracle state at its timestamp.",
        "This baseline does not establish campaign-selector 1 to entry-9 parity.",
    ],
}
(output / "manifest.json").write_text(json.dumps(payload, indent=2) + "\n")
PY
