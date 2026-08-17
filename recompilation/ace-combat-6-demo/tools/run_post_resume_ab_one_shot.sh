#!/usr/bin/env bash
set -euo pipefail

export TMPDIR=/fastdata/lavaulta/tmp

if (( $# != 0 )); then
  printf 'usage: %s\n' "${BASH_SOURCE[0]}" >&2
  exit 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT="$(cd -- "$SCRIPT_DIR/.." && pwd -P)"
WORKSPACE="$(cd -- "$PROJECT/../.." && pwd -P)"

EXPECTED_XEX_SHA256="de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
EXPECTED_BASEFILE_SHA256="b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218"
EXPECTED_CODEGEN_MANIFEST_SHA256="9f1fffb0398358331f9bbf575a3d2fb5cf1478f7cbda5a1dbe46c264a935bbfa"
EXPECTED_GHIDRA_MANIFEST_SHA256="576fa31e02b1c899cdc997b8a6e252d6d7785656d13067a9d8a54aeb2810086c"
EXPECTED_BOUNDARY_CONFIG_SHA256="4a87dd8c377638f6bddf308ee63025e314c5a3470507767070e2ad15c0e54506"
EXPECTED_BINARY_SHA256="bf35ab5212e0d7c797bc28c5ad4256ea4948116d9f4d4c7770a9aa42babc1964"
EXPECTED_XENONRECOMP_COMMIT="ddd128bcca99fe8bfbb99bea583c972351fa6ace"

XEX="$WORKSPACE/demo-game-file/extracted/stfs-root/Default.xex"
BUILD="$PROJECT/build-codegen-on"
BINARY="$BUILD/ac6-demo-recomp"
CODEGEN="$BUILD/codegen"
BASEFILE="$CODEGEN/xex-basefile.bin"
CODEGEN_MANIFEST="$CODEGEN/manifest.json"
BUILD_IDENTITY="$CODEGEN/ac6_demo_build_identity.hpp"
GENERATED="$CODEGEN/generated"
GENERATED_ADAPTER="$CODEGEN/ppc_context_adapter.h"
ADAPTER="$PROJECT/tools/ppc_context_adapter.h"
GHIDRA_MANIFEST="$WORKSPACE/analysis/demo/ac6-demo-ghidra-manifest.json"
GHIDRA_PROJECT="$WORKSPACE/ghidra-projects/ace-combat-6-demo.rep"
BOUNDARY_CONFIG="$PROJECT/config/confirmed-chunks.toml"
CMAKE_CACHE="$BUILD/CMakeCache.txt"
STORE_SEED="$WORKSPACE/.build/ac6-demo-atomic-start-vulkan-store"
MAPPER="$PROJECT/tools/map_generated_guest_load_sites.py"
OUTPUT_ROOT="$WORKSPACE/analysis/demo/ac6-demo-post-resume-ab"

MAX_REPORT_BYTES=$((32 * 1024 * 1024))
MAX_TRACE_BYTES=$((128 * 1024 * 1024))
MAX_STDOUT_BYTES=$((8 * 1024 * 1024))
MAX_STDERR_BYTES=$((32 * 1024 * 1024))
CAPTURE_FILE_MAX_BYTES="$MAX_TRACE_BYTES"
PROBE_TIMEOUT_SECONDS=900
XVFB_START_TIMEOUT_SECONDS=10

IDENTITY_VERIFIED=0
CURRENT_STAGE="preflight"
FAIL_REASON=""
ACTUAL_XEX_SHA256=""
ACTUAL_BASEFILE_SHA256=""
ACTUAL_CODEGEN_MANIFEST_SHA256=""
ACTUAL_GHIDRA_MANIFEST_SHA256=""
ACTUAL_BOUNDARY_CONFIG_SHA256=""
ACTUAL_BINARY_SHA256=""
ACTUAL_ADAPTER_SHA256=""
RUNNER_AUX_FAILURE=""

die() {
  FAIL_REASON="$*"
  printf 'post-resume A/B refused: %s\n' "$*" >&2
  exit 1
}

require_file() {
  local path="$1"
  [[ -f "$path" && ! -L "$path" ]] || die "missing or symlinked file: $path"
}

require_dir() {
  local path="$1"
  [[ -d "$path" && ! -L "$path" ]] || die "missing or symlinked directory: $path"
}

reject_historical_path() {
  local path="$1"
  case "$path" in
    *retail*|*Retail*|*historical*|*Historical*|*corrected*|*Corrected*)
      die "retail or historical path refused: $path"
      ;;
  esac
}

sha256_file() {
  sha256sum -- "$1" | awk '{print $1}'
}

require_bounded_file() {
  local path="$1"
  local maximum="$2"
  local size
  require_file "$path"
  size="$(stat -c '%s' -- "$path")"
  (( size <= maximum )) ||
    die "bounded artifact exceeds $maximum bytes: $path ($size)"
}

trees_identical() { diff -qr --no-dereference -- "$1" "$2" >/dev/null 2>&1; }

publish_tree() {
  local source="$1" root="$2" receipt="$1/receipt.json" digest destination
  [[ -f "$receipt" && ! -L "$receipt" ]] || return 1
  (( $(stat -c '%s' -- "$receipt") <= MAX_REPORT_BYTES )) || return 1
  digest="$(sha256_file "$receipt")"; [[ "$digest" =~ ^[0-9a-f]{64}$ ]] || return 1
  destination="$root/sha256/$digest"
  if [[ -e "$root" || -L "$root" ]]; then [[ -d "$root" && ! -L "$root" ]] || return 1; fi
  mkdir -p -- "$root/sha256"
  if [[ -e "$destination" || -L "$destination" ]]; then
    [[ -d "$destination" && ! -L "$destination" ]] || return 1
    trees_identical "$source" "$destination" || return 1
    rm -rf -- "$source"; return 0
  fi
  if mv -T --no-copy -- "$source" "$destination"; then return 0; fi
  [[ -d "$destination" && ! -L "$destination" ]] && trees_identical "$source" "$destination" || return 1
  rm -rf -- "$source"
}

publish_failure_capsule() {
  local stage="${CURRENT_STAGE:-unknown}" reason="${FAIL_REASON:-command failed at stage ${CURRENT_STAGE:-unknown}}"
  local failure_tmp="$RUN_TMP/failure-staging" index="$RUN_TMP/failure-index.tsv" routes="$RUN_TMP/failure-routes.tsv"
  local route source label limit size sha copied status
  mkdir -p -- "$failure_tmp/artifacts" || return 1
  : >"$index" || return 1; : >"$routes" || return 1
  for route in neutral buttons_16; do
    if [[ -f "$RUN_TMP/$route/return_code" ]]; then
      printf '%s\t%s\t%s\t%s\n' "$route" "$(<"$RUN_TMP/$route/return_code")" \
        "$(grep -a -c '^AC6_POST_RESUME_INSTRUCTION_HANDOFF ' "$RUN_TMP/$route/$route.stderr.log" 2>/dev/null || true)" \
        "$(grep -a -c '^AC6_POST_RESUME_ACCESS ' "$RUN_TMP/$route/$route.stderr.log" 2>/dev/null || true)" >>"$routes"
    else
      printf '%s\tnot_started\tnull\tnull\n' "$route" >>"$routes"
    fi
    for spec in \
      "report:$route/$route.report.json:$MAX_REPORT_BYTES" \
      "trace:$route/$route.trace:$MAX_TRACE_BYTES" \
      "stderr:$route/$route.stderr.log:$MAX_STDERR_BYTES" \
      "raw_post_resume:$route/$route.stderr.log:$MAX_STDERR_BYTES" \
      "stdout:$route/$route.stdout.log:$MAX_STDOUT_BYTES" \
      "mapper_stderr:$route.mapper.stderr:$MAX_STDERR_BYTES"; do
      IFS=: read -r label source limit <<<"$spec"
      source="$RUN_TMP/$source"
      if [[ -f "$source" && ! -L "$source" ]]; then
        size="$(stat -c '%s' -- "$source")"; sha="$(sha256_file "$source")"; copied=false; status=present
        if (( size <= limit )); then
          mkdir -p -- "$failure_tmp/artifacts/$route"
          if ! cp -- "$source" "$failure_tmp/artifacts/$route/$label"; then return 1; fi
          copied=true
        else status=omitted_quota; fi
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$route" "$label" "$status" "$size" "$sha" "$copied" "$limit" >>"$index"
      else
        printf '%s\t%s\tmissing\t0\t\tfalse\t%s\n' "$route" "$label" "$limit" >>"$index"
      fi
    done
  done
  if ! python3 - "$failure_tmp/receipt.json" "$index" "$routes" "$stage" "$reason" \
    "$ACTUAL_XEX_SHA256" "$ACTUAL_BASEFILE_SHA256" "$ACTUAL_CODEGEN_MANIFEST_SHA256" \
    "$ACTUAL_GHIDRA_MANIFEST_SHA256" "$ACTUAL_BOUNDARY_CONFIG_SHA256" "$ACTUAL_BINARY_SHA256" "$ACTUAL_ADAPTER_SHA256" <<'PY'
import json, sys
from pathlib import Path
out, index, routes, stage, reason, *identity = sys.argv[1:]
items = {}
for line in Path(index).read_text().splitlines():
    r, label, status, size, sha, copied, limit = line.split("\t")
    items.setdefault(r, {})[label] = {"status": status, "bytes": int(size), "sha256": sha or None,
        "path": f"artifacts/{r}/{label}" if copied == "true" else None,
        "truncated": False, "quota_exceeded": status == "omitted_quota", "quota_bytes": int(limit)}
route_data = {}
for line in Path(routes).read_text().splitlines():
    r, rc, handoff, access = line.split("\t")
    route_data[r] = {"return_code": None if rc == "not_started" else int(rc),
                     "handoff_count": None if handoff == "null" else int(handoff),
                     "access_count": None if access == "null" else int(access)}
receipt = {"schema":"ac6-demo-post-resume-ab-failure/v1", "status":"failed", "stage":stage, "reason":reason,
 "target":{"project":"ace-combat-6-demo","module":"Default.xex","xex_sha256":identity[0],"basefile_sha256":identity[1],"architecture":"Xenon big-endian / Xenos"},
 "identity":{"qualified":True,"xex_sha256":identity[0],"basefile_sha256":identity[1],"codegen_manifest_sha256":identity[2],"ghidra_manifest_sha256":identity[3],"boundary_config_sha256":identity[4],"binary_sha256":identity[5],"adapter_sha256":identity[6]},
 "routes":route_data,"artifacts":items,"quotas":{"truncation":False,"capture_file_bytes":134217728,"process_timeout_seconds":900},
 "classification":{"demo-observed":["bounded failure evidence only"],"unknown":["frontend and native parity"]},
 "policy":{"frontend_promoted":False,"native_parity_promoted":False,"fail_closed":True}}
Path(out).write_text(json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n")
PY
  then return 1; fi
  local failure_digest
  failure_digest="$(sha256_file "$failure_tmp/receipt.json")"
  publish_tree "$failure_tmp" "$OUTPUT_ROOT/failures" || return 1
  printf 'post-resume A/B failure capsule: %s/failures/sha256/%s/receipt.json\n' \
    "$OUTPUT_ROOT" "$failure_digest" >&2
}

require_dir "$PROJECT"
[[ "$(basename -- "$PROJECT")" == "ace-combat-6-demo" ]] ||
  die "unexpected product project: $PROJECT"
[[ "$PROJECT" != *-corrected* && "$PROJECT" != *historical* ]] ||
  die "historical product project refused: $PROJECT"

for path in "$XEX" "$BINARY" "$BASEFILE" "$CODEGEN_MANIFEST" \
            "$BUILD_IDENTITY" "$GENERATED_ADAPTER" "$ADAPTER" \
            "$GHIDRA_MANIFEST" "$GHIDRA_PROJECT" "$BOUNDARY_CONFIG" "$CMAKE_CACHE" \
            "$MAPPER"; do
  reject_historical_path "$path"
done
require_file "$XEX"
require_file "$BINARY"
require_file "$BASEFILE"
require_file "$CODEGEN_MANIFEST"
require_file "$BUILD_IDENTITY"
require_file "$GENERATED_ADAPTER"
require_file "$ADAPTER"
require_file "$GHIDRA_MANIFEST"
require_dir "$GHIDRA_PROJECT"
require_file "$BOUNDARY_CONFIG"
require_file "$CMAKE_CACHE"
require_file "$MAPPER"
require_dir "$BUILD"
require_dir "$CODEGEN"
require_dir "$GENERATED"
require_dir "$STORE_SEED"
require_file "$STORE_SEED/.ac6-demo-store"
require_file "$STORE_SEED/Default.xex"
command -v python3 >/dev/null 2>&1 || die "python3 is required"
command -v sha256sum >/dev/null 2>&1 || die "sha256sum is required"
command -v Xvfb >/dev/null 2>&1 || die "Xvfb is required"
command -v setsid >/dev/null 2>&1 || die "setsid is required"
command -v strings >/dev/null 2>&1 || die "strings is required"
command -v timeout >/dev/null 2>&1 || die "timeout is required"

ACTUAL_XEX_SHA256="$(sha256_file "$XEX")"; [[ "$ACTUAL_XEX_SHA256" == "$EXPECTED_XEX_SHA256" ]] ||
  die "the canonical demo XEX is not the qualified PAL demo"
ACTUAL_BASEFILE_SHA256="$(sha256_file "$BASEFILE")"; [[ "$ACTUAL_BASEFILE_SHA256" == "$EXPECTED_BASEFILE_SHA256" ]] ||
  die "the codegen basefile is not the qualified PAL basefile"
ACTUAL_CODEGEN_MANIFEST_SHA256="$(sha256_file "$CODEGEN_MANIFEST")"; [[ "$ACTUAL_CODEGEN_MANIFEST_SHA256" == "$EXPECTED_CODEGEN_MANIFEST_SHA256" ]] ||
  die "the codegen manifest is not the current qualified manifest"
ACTUAL_GHIDRA_MANIFEST_SHA256="$(sha256_file "$GHIDRA_MANIFEST")"; [[ "$ACTUAL_GHIDRA_MANIFEST_SHA256" == "$EXPECTED_GHIDRA_MANIFEST_SHA256" ]] ||
  die "the Ghidra manifest is not the current qualified demo manifest"
ACTUAL_BOUNDARY_CONFIG_SHA256="$(sha256_file "$BOUNDARY_CONFIG")"; [[ "$ACTUAL_BOUNDARY_CONFIG_SHA256" == "$EXPECTED_BOUNDARY_CONFIG_SHA256" ]] ||
  die "the boundary configuration is not the current qualified configuration"
cmp -- "$GENERATED_ADAPTER" "$ADAPTER" ||
  die "generated adapter differs from the current source adapter"
cmp -- "$STORE_SEED/Default.xex" "$XEX" ||
  die "store seed XEX differs from the qualified demo XEX"
grep -Fxq 'AC6-DEMO-STORE-v1' "$STORE_SEED/.ac6-demo-store" ||
  die "store seed marker is not AC6-DEMO-STORE-v1"
grep -Fxq "$EXPECTED_XEX_SHA256" "$STORE_SEED/.ac6-demo-store" ||
  die "store seed marker does not qualify the demo XEX"
grep -Fxq 'AC6_DEMO_ENABLE_CODEGEN:BOOL=ON' "$CMAKE_CACHE" ||
  die "build is not codegen-ON"
grep -Fxq "AC6_DEMO_XEX:FILEPATH=$XEX" "$CMAKE_CACHE" ||
  die "build cache is not configured for the canonical demo XEX"
grep -Fxq "ac6_demo_recomp_SOURCE_DIR:STATIC=$PROJECT" "$CMAKE_CACHE" ||
  die "build cache is not configured for ace-combat-6-demo"
grep -Fxq "ac6_demo_recomp_BINARY_DIR:STATIC=$BUILD" "$CMAKE_CACHE" ||
  die "build cache is not the current codegen-ON build"

python3 - "$CODEGEN_MANIFEST" "$BUILD_IDENTITY" "$GHIDRA_MANIFEST" \
        "$EXPECTED_XEX_SHA256" "$EXPECTED_CODEGEN_MANIFEST_SHA256" \
        "$EXPECTED_GHIDRA_MANIFEST_SHA256" \
        "$EXPECTED_BOUNDARY_CONFIG_SHA256" "$EXPECTED_XENONRECOMP_COMMIT" <<'PY'
import json
import sys
from pathlib import Path

manifest_path = Path(sys.argv[1])
identity_path = Path(sys.argv[2])
ghidra_path = Path(sys.argv[3])
expected_xex = sys.argv[4]
expected_codegen = sys.argv[5]
expected_ghidra = sys.argv[6]
expected_boundary = sys.argv[7]
expected_recomp = sys.argv[8]

manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
if manifest.get("schema") != "ac6-demo-codegen-manifest/v2":
    raise SystemExit("unexpected codegen manifest schema")
if manifest.get("target_id") != "ac6-demo-xbox360-pal":
    raise SystemExit("unexpected codegen target")
if manifest.get("xex_sha256") != expected_xex:
    raise SystemExit("codegen manifest XEX mismatch")
if manifest.get("ghidra_manifest_sha256") != expected_ghidra:
    raise SystemExit("codegen manifest Ghidra mismatch")
if manifest.get("xenonrecomp_commit") != expected_recomp:
    raise SystemExit("codegen toolchain commit mismatch")
if manifest.get("boundary_diagnostics") != 0 or manifest.get("unsupported_instructions") != 0:
    raise SystemExit("codegen manifest contains diagnostics")
if manifest.get("generated_output") != "build-only":
    raise SystemExit("generated output is not build-only")
if not isinstance(manifest.get("generated_cpp_files"), int) or manifest["generated_cpp_files"] <= 0:
    raise SystemExit("codegen manifest has no generated C++ units")

ghidra = json.loads(ghidra_path.read_text(encoding="utf-8"))
if ghidra.get("schema") != "ac6-demo-ghidra-chunks.v2":
    raise SystemExit("unexpected Ghidra manifest schema")
if ghidra.get("project") != "ace-combat-6-demo":
    raise SystemExit("historical or wrong Ghidra project")
if ghidra.get("program") != "Default.xex" or ghidra.get("module") != "Default.xex":
    raise SystemExit("wrong Ghidra module")
if ghidra.get("target_id") != "ac6-demo-xbox360-pal" or ghidra.get("xex_sha256") != expected_xex:
    raise SystemExit("Ghidra manifest target mismatch")

identity = identity_path.read_text(encoding="utf-8")
required_identity = (
    f'kCodegenManifestSha256 = "{expected_codegen}"',
    f'kGhidraManifestSha256 = "{expected_ghidra}"',
    f'kBoundaryConfigSha256 = "{expected_boundary}"',
)
for needle in required_identity:
    if needle not in identity:
        raise SystemExit(f"build identity missing {needle}")
PY

[[ -x "$BINARY" ]] || die "runtime binary is not executable"
strings -a "$BINARY" | grep -Fx 'AC6_DEMO_WATCH_POST_RESUME_ACCESS' >/dev/null ||
  die "runtime binary lacks the post-resume watcher"
strings -a "$BINARY" | grep -F 'AC6_POST_RESUME_INSTRUCTION_HANDOFF' >/dev/null ||
  die "runtime binary lacks the post-resume handoff record"

# A codegen-ON binary newer than every source/config/generated input is the
# only accepted build freshness signal. The runner never rebuilds it.
for root in "$PROJECT/src" "$PROJECT/include" "$PROJECT/config" "$GENERATED"; do
  require_dir "$root"
done
stale_input="$(find "$PROJECT/src" "$PROJECT/include" "$PROJECT/config" \
  "$PROJECT/tools/ppc_context_adapter.h" "$CODEGEN_MANIFEST" \
  "$BUILD_IDENTITY" "$GENERATED_ADAPTER" "$GENERATED" \
  -type f -newer "$BINARY" -print -quit)"
[[ -z "$stale_input" ]] || die "runtime binary is older than input: $stale_input"
ACTUAL_BINARY_SHA256="$(sha256_file "$BINARY")"; [[ "$ACTUAL_BINARY_SHA256" == "$EXPECTED_BINARY_SHA256" ]] ||
  die "runtime binary is not the current qualified codegen-ON binary"
ACTUAL_ADAPTER_SHA256="$(sha256_file "$ADAPTER")"
IDENTITY_VERIFIED=1

RUN_TMP=""
cleanup() {
  local status="$?"
  if (( status != 0 )) && (( IDENTITY_VERIFIED == 1 )) && [[ -n "${RUN_TMP:-}" && -d "$RUN_TMP" ]]; then
    set +e
    publish_failure_capsule || printf 'post-resume failure capsule publication failed; evidence was not published\n' >&2
    set -e
  fi
  if [[ -n "${RUN_TMP:-}" && -d "$RUN_TMP" ]]; then
    rm -rf -- "$RUN_TMP"
  fi
  exit "$status"
}
trap cleanup EXIT
RUN_TMP="$(mktemp -d "$TMPDIR/ac6-demo-post-resume-ab.XXXXXX")"
STAGING="$RUN_TMP/staging"
mkdir -- "$STAGING"
OUTPUT_DIR="$STAGING"
[[ "$(stat -c '%d' "$RUN_TMP")" == "$(stat -c '%d' "$WORKSPACE")" ]] ||
  die "TMPDIR and workspace are on different filesystems; atomic publish refused"

# Do not allow a caller's unrelated AC6_DEMO_* diagnostics to contaminate the
# two runs. The only runtime diagnostics reintroduced below are the cycle-1756
# XMA create/kick gates and this one-shot post-resume watcher.
while IFS='=' read -r variable _; do
  if [[ "$variable" == AC6_DEMO_* ]]; then
    unset "$variable"
  fi
done < <(env)

copy_store() {
  local destination="$1"
  mkdir -- "$destination"
  cp -a --reflink=auto -- "$STORE_SEED/." "$destination/"
  cmp -- "$destination/Default.xex" "$XEX" ||
    die "fresh store does not contain the qualified demo XEX: $destination"
  grep -Fxq "$EXPECTED_XEX_SHA256" "$destination/.ac6-demo-store" ||
    die "fresh store marker is not qualified: $destination"
}

run_capped_probe() {
  local return_path="$1"; shift
  local file_limit_blocks=$(( (CAPTURE_FILE_MAX_BYTES + 511) / 512 ))
  local xvfb_dir="$RUN_TMP/xvfb-${RANDOM}-${RANDOM}" display_file xvfb_pid display display_number socket_path socket_preexisting=0
  local xvfb_control_fd xvfb_output_fd xvfb_status_file
  local runtime_rc=125 deadline pid_stat pid_start pid_pgid
  local supervisor_code

  # This supervisor owns the Xvfb child. Its control pipe is an ownership
  # handle that remains safe when /proc cannot validate a PID. The supervisor
  # and child share a fresh session/process group for the validated fast path.
  IFS= read -r -d '' supervisor_code <<'PY' || true
import os
import select
import signal
import subprocess
import sys
import time

display_file, status_file, ready_file, term_grace, kill_grace = sys.argv[1:]
term_grace = float(term_grace)
kill_grace = float(kill_grace)
child = None
child_starttime = None
child_pgid = None
stop_requested = False


def proc_identity(pid):
    try:
        raw = open(f"/proc/{pid}/stat", encoding="ascii").read()
    except OSError:
        return None
    close = raw.rfind(")")
    if close < 0:
        return None
    fields = raw[close + 2:].split()
    if len(fields) < 20:
        return None
    try:
        return fields[0], int(fields[2]), int(fields[19])
    except ValueError:
        return None


def write_status(value):
    temporary = f"{status_file}.tmp"
    with open(temporary, "w", encoding="ascii") as stream:
        stream.write(value + "\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, status_file)


def group_is_owned():
    identity = proc_identity(child.pid)
    return (
        identity is not None and identity[0] != "Z"
        and identity[1] == child_pgid
        and identity[2] == child_starttime
        and child_pgid == child.pid
    )


def signal_child(number):
    if child is None or child.poll() is not None:
        return
    if group_is_owned():
        os.killpg(child_pgid, number)
    else:
        # child is our direct, unreaped child, so its PID cannot be reused.
        os.kill(child.pid, number)


def signal_until_reaped(number, grace):
    failed = False
    try:
        signal_child(number)
    except ProcessLookupError:
        pass
    deadline = time.monotonic() + grace
    while child.poll() is None and time.monotonic() < deadline:
        time.sleep(0.02)
    if child.poll() is None:
        failed = True
    return failed


def stop_child():
    signal_until_reaped(signal.SIGTERM, term_grace)
    if child.poll() is None:
        return signal_until_reaped(signal.SIGKILL, kill_grace)
    return False


def on_term(_number, _frame):
    global stop_requested
    stop_requested = True


signal.signal(signal.SIGTERM, on_term)
signal.signal(signal.SIGHUP, on_term)
signal.signal(signal.SIGINT, on_term)
display_fd = os.open(display_file, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
if display_fd != 3:
    # An inherited fd 3 (ctest keeps its log open there) would otherwise be
    # dup2'd over in preexec_fn and then closed again by close_fds, since a
    # freshly duplicated 3 is not in pass_fds. Pin the display file to 3 in
    # this supervisor before forking so pass_fds keeps exactly that fd.
    os.dup2(display_fd, 3)
    os.close(display_fd)
    display_fd = 3
stdout = open(os.path.join(os.path.dirname(status_file), "xvfb.stdout"), "wb")
stderr = open(os.path.join(os.path.dirname(status_file), "xvfb.stderr"), "wb")


try:
    child = subprocess.Popen(
        ["Xvfb", "-displayfd", "3", "-screen", "0", "1280x720x24", "-nolisten", "tcp"],
        stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
        close_fds=True, pass_fds=(display_fd,))
    os.close(display_fd)
    stdout.close()
    stderr.close()
    identity = proc_identity(child.pid)
    if identity is not None:
        child_starttime = identity[2]
        child_pgid = identity[1]
    with open(ready_file, "w", encoding="ascii") as marker:
        marker.write("ready\n")
        marker.flush()
        os.fsync(marker.fileno())

    while True:
        if stop_requested:
            write_status("cleanup_failed" if stop_child() else "stopped")
            break
        if child.poll() is not None:
            write_status("child_exited")
            break
        readable, _, _ = select.select([sys.stdin], [], [], 0.05)
        if readable:
            command = sys.stdin.readline()
            if not command or command.strip() in {"STOP", "TERM", "KILL"}:
                stop_requested = True
except Exception:
    write_status("child_exited")
finally:
    try:
        if child is not None and child.poll() is None:
            write_status("cleanup_failed" if stop_child() else "stopped")
    except (OSError, ProcessLookupError):
        try:
            write_status("cleanup_failed")
        except OSError:
            pass
    for stream in (stdout, stderr):
        try:
            stream.close()
        except Exception:
            pass
PY

  mkdir -- "$xvfb_dir" || { RUNNER_AUX_FAILURE="xvfb staging failed"; return 125; }
  display_file="$xvfb_dir/display"
  xvfb_status_file="$xvfb_dir/status"
  : >"$display_file"
  unset AC6_XVFB_OWNER AC6_XVFB_OWNER_PID
  coproc AC6_XVFB_OWNER {
    exec setsid python3 -c "$supervisor_code" \
      "$display_file" "$xvfb_status_file" "$xvfb_dir/ready" 1.5 1.5 \
      >"$xvfb_dir/supervisor.stdout" 2>"$xvfb_dir/supervisor.stderr"
  }
  xvfb_pid="$AC6_XVFB_OWNER_PID"
  xvfb_control_fd="${AC6_XVFB_OWNER[1]}"
  xvfb_output_fd="${AC6_XVFB_OWNER[0]}"
  exec {xvfb_output_fd}<&-
  # From this fork onward every branch invokes cleanup_owned_xvfb exactly once.
  pid_stat="/proc/$xvfb_pid/stat"
  pid_start=""
  pid_pgid=""
  if [[ -r "$pid_stat" ]]; then
    pid_start="$(awk '{print $22}' "$pid_stat" 2>/dev/null || true)"
    pid_pgid="$(awk '{print $5}' "$pid_stat" 2>/dev/null || true)"
  fi
  xvfb_control_fd_open=1

  xvfb_identity_ok() {
    case "${AC6_TEST_XVFB_IDENTITY_MODE:-}" in
      absent|mismatch) return 1 ;;
    esac
    [[ -n "${pid_start:-}" && -n "${pid_pgid:-}" ]] || return 1
    [[ "$pid_pgid" == "$xvfb_pid" ]] || return 1
    [[ -r "/proc/$xvfb_pid/stat" ]] || return 1
    [[ "$(awk '{print $3}' "/proc/$xvfb_pid/stat" 2>/dev/null || true)" != Z ]] || return 1
    [[ "$(awk '{print $22}' "/proc/$xvfb_pid/stat" 2>/dev/null || true)" == "$pid_start" ]] || return 1
    [[ "$(awk '{print $5}' "/proc/$xvfb_pid/stat" 2>/dev/null || true)" == "$pid_pgid" ]]
  }

  xvfb_reaped_or_zombie() {
    if [[ -r "/proc/$xvfb_pid/stat" ]]; then
      [[ "$(awk '{print $3}' "/proc/$xvfb_pid/stat" 2>/dev/null || true)" == Z ]]
      return
    fi
    [[ -f "$xvfb_status_file" ]] && ! kill -0 "$xvfb_pid" 2>/dev/null && return 0
    return 1
  }

  xvfb_pidfd_exited() {
    local timeout_ms="$1"
    [[ -f "$xvfb_status_file" ]] || return 1
    if ! kill -0 "$xvfb_pid" 2>/dev/null; then return 0; fi
    python3 - "$xvfb_pid" "$timeout_ms" <<'PY'
import os
import select
import sys

try:
    pidfd = os.pidfd_open(int(sys.argv[1]))
except (AttributeError, OSError, ValueError):
    raise SystemExit(1)
try:
    poller = select.poll()
    poller.register(pidfd, select.POLLIN)
    raise SystemExit(0 if poller.poll(max(0, int(sys.argv[2]))) else 1)
finally:
    os.close(pidfd)
PY
  }

  cleanup_owned_xvfb() {
    local cleanup_status=0 cleanup_deadline had_errexit=0 prior_failure
    prior_failure="${RUNNER_AUX_FAILURE:-}"
    case $- in *e*) had_errexit=1 ;; esac
    set +e
    # Pipe/control is the safe fallback when starttime+pgid is unavailable.
    if (( ${xvfb_control_fd_open:-0} == 1 )); then
      if [[ -e "/proc/$$/fd/$xvfb_control_fd" ]]; then
        printf 'STOP\n' >&"$xvfb_control_fd" || true
      fi
    fi
    if [[ -f "$xvfb_dir/ready" ]] && xvfb_identity_ok; then
      if ! kill -TERM -- "-$pid_pgid" 2>/dev/null; then
        [[ -f "$xvfb_status_file" ]] || cleanup_status=1
      fi
    fi
    cleanup_deadline=$((SECONDS + 4))
    while ! xvfb_reaped_or_zombie && (( SECONDS < cleanup_deadline )); do sleep 0.05; done
    if ! xvfb_reaped_or_zombie && xvfb_identity_ok; then
      # Revalidate after TERM; a changed/missing identity is never signalled.
      if ! kill -KILL -- "-$pid_pgid" 2>/dev/null; then
        [[ -f "$xvfb_status_file" ]] || cleanup_status=1
      fi
      cleanup_deadline=$((SECONDS + 2))
      while ! xvfb_reaped_or_zombie && (( SECONDS < cleanup_deadline )); do sleep 0.05; done
    fi
    cleanup_deadline=$((SECONDS + 1))
    if xvfb_reaped_or_zombie || xvfb_pidfd_exited $(( (cleanup_deadline - SECONDS) * 1000 )); then
      # /proc-Z, pidfd-POLLIN, or kill-0 failure has already observed exit;
      # status is published only after the supervisor's bounded Popen.poll()
      # reap. The builtin wait is therefore an immediate zombie reap, never
      # a wait while a live child remains.
      wait "$xvfb_pid" 2>/dev/null || true
    else
      cleanup_status=1
    fi
    if (( ${xvfb_control_fd_open:-0} == 1 )); then
      if [[ -e "/proc/$$/fd/$xvfb_control_fd" ]]; then
        eval "exec ${xvfb_control_fd}>&-" || cleanup_status=1
      fi
      xvfb_control_fd_open=0
    fi
    (( socket_preexisting == 1 )) || [[ ! -e "${socket_path:-}" ]] || cleanup_status=1
    rm -rf -- "$xvfb_dir" || cleanup_status=1
    [[ "${AC6_TEST_XVFB_CLEANUP_FAILURE:-0}" == 1 ]] && cleanup_status=1
    if (( cleanup_status != 0 )) && [[ -z "$prior_failure" ]]; then
      RUNNER_AUX_FAILURE="xvfb cleanup failed"
    fi
    if (( had_errexit == 1 )); then set -e; fi
  }

  deadline=$((SECONDS + XVFB_START_TIMEOUT_SECONDS))
  while [[ ! -s "$display_file" && ! -f "$xvfb_status_file" && $SECONDS -lt $deadline ]]; do sleep 0.1; done
  if [[ ! -s "$display_file" ]]; then
    RUNNER_AUX_FAILURE="xvfb start failed"
    cleanup_owned_xvfb
    return 125
  fi
  display_number="$(python3 - "$display_file" <<'PY'
import re
import sys
from pathlib import Path

try:
    raw = Path(sys.argv[1]).read_text(encoding="ascii")
    if raw.count("\n") != 1 or not raw.endswith("\n"):
        raise ValueError
    value = raw[:-1]
    if not re.fullmatch(r"[0-9]+", value):
        raise ValueError
    number = int(value)
    if number > 65535:
        raise ValueError
except (OSError, UnicodeError, ValueError):
    raise SystemExit(1)
print(number)
PY
)" || {
    RUNNER_AUX_FAILURE="xvfb display allocation failed"
    cleanup_owned_xvfb
    return 125
  }
  display=":$display_number"
  socket_path="/tmp/.X11-unix/X${display_number}"
  [[ -e "$socket_path" ]] && socket_preexisting=1
  set +e
  DISPLAY="$display" timeout --signal=TERM --kill-after=5s "${PROBE_TIMEOUT_SECONDS}s" \
    setsid bash -c '
      set -euo pipefail
      ulimit -f "$1"; shift
      export SDL_AUDIODRIVER=dummy AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1
      export AC6_DEMO_EXPERIMENTAL_XMA_KICK=1 AC6_DEMO_WATCH_POST_RESUME_ACCESS=1
      exec "$@"
    ' _ "$file_limit_blocks" "$@"
  runtime_rc=$?
  printf '%s\n' "$runtime_rc" >"$return_path" || RUNNER_AUX_FAILURE="xvfb return record failed"
  cleanup_owned_xvfb
  set -e
  return "$runtime_rc"
}

run_route() {
  local route="$1"
  local input_at="${2:-}"
  local route_tmp="$RUN_TMP/$route"
  local store="$route_tmp/store"
  local trace="$route_tmp/$route.trace"
  local report="$route_tmp/$route.report.json"
  local stdout="$route_tmp/$route.stdout.log"
  local stderr="$route_tmp/$route.stderr.log"
  local return_code
  local -a probe=(
    "$BINARY" probe
    --store "$store"
    --until frontend
    --max-ticks 5600
    --trace "$trace"
    --report "$report"
    --backend vulkan
  )

  RUNNER_AUX_FAILURE=""
  mkdir -- "$route_tmp"
  copy_store "$store"
  if [[ -n "$input_at" ]]; then
    probe+=(--input-at "$input_at")
  fi

  if run_capped_probe "$route_tmp/return_code" "${probe[@]}" >"$stdout" 2>"$stderr"; then
    return_code=0
  else
    return_code=$?
  fi
  if [[ -n "$RUNNER_AUX_FAILURE" ]]; then
    CURRENT_STAGE="xvfb.$route"
    die "$RUNNER_AUX_FAILURE (guest rc=$return_code)"
  fi
  return_code="$(<"$route_tmp/return_code")"
  [[ "$return_code" -eq 4 ]] ||
    die "$route did not finish at max_ticks with rc 4 (rc=$return_code)"

  require_bounded_file "$report" "$MAX_REPORT_BYTES"
  require_bounded_file "$trace" "$MAX_TRACE_BYTES"
  require_bounded_file "$stdout" "$MAX_STDOUT_BYTES"
  require_bounded_file "$stderr" "$MAX_STDERR_BYTES"
  local capture capture_size
  for capture in "$report" "$trace" "$stdout" "$stderr"; do
    capture_size="$(stat -c '%s' -- "$capture")"
    (( capture_size < CAPTURE_FILE_MAX_BYTES )) ||
      die "$route reached the live capture quota without publishing evidence: $capture"
  done
  local handoff_count access_count refused_count
  handoff_count="$(grep -a -c '^AC6_POST_RESUME_INSTRUCTION_HANDOFF ' "$stderr" || true)"
  access_count="$(grep -a -c '^AC6_POST_RESUME_ACCESS ' "$stderr" || true)"
  refused_count="$(grep -a -c '^AC6_POST_RESUME_ACCESS_REFUSED ' "$stderr" || true)"
  [[ "$handoff_count" -eq 1 ]] ||
    die "$route emitted $handoff_count post-resume handoffs (expected exactly 1)"
  (( access_count <= 1 )) ||
    die "$route emitted $access_count post-resume accesses (maximum is 1)"
  [[ "$refused_count" -eq 0 ]] ||
    die "$route emitted a refused post-resume access"

  cp -- "$report" "$OUTPUT_DIR/$route.report.json"
  cp -- "$trace" "$OUTPUT_DIR/$route.trace"
  cp -- "$stdout" "$OUTPUT_DIR/$route.stdout.log"
  cp -- "$stderr" "$OUTPUT_DIR/$route.stderr.log"
  printf '%s\n' "$return_code" >"$OUTPUT_DIR/$route.return_code"
  printf '%s\n' "$handoff_count" >"$OUTPUT_DIR/$route.handoff_count"
  printf '%s\n' "$access_count" >"$OUTPUT_DIR/$route.access_count"
}

CURRENT_STAGE="capture.neutral"; run_route neutral
CURRENT_STAGE="capture.buttons_16"; run_route buttons_16 '252,16,0,0,0,0,0,0,1'

map_route() {
  local route="$1"
  local stderr="$OUTPUT_DIR/$route.stderr.log"
  local mapping="$OUTPUT_DIR/$route.post-resume-map.json"
  local mapper_stdout="$RUN_TMP/$route.mapper.stdout"
  local mapper_stderr="$RUN_TMP/$route.mapper.stderr"
  if ! python3 "$MAPPER" \
      --generated-dir "$GENERATED" \
      --manifest "$CODEGEN_MANIFEST" \
      --basefile "$BASEFILE" \
      --xex "$XEX" \
      --log "$stderr" \
      --output "$mapping" >"$mapper_stdout" 2>"$mapper_stderr"; then
    cat -- "$mapper_stderr" >&2
    die "$route post-resume mapper failed closed"
  fi
  require_bounded_file "$mapping" "$MAX_REPORT_BYTES"
  require_bounded_file "$mapper_stdout" "$MAX_STDOUT_BYTES"
  require_bounded_file "$mapper_stderr" "$MAX_STDERR_BYTES"
  cp -- "$mapper_stdout" "$OUTPUT_DIR/$route.mapper.stdout.log"
  cp -- "$mapper_stderr" "$OUTPUT_DIR/$route.mapper.stderr.log"
}

CURRENT_STAGE="mapping.neutral"; map_route neutral
CURRENT_STAGE="mapping.buttons_16"; map_route buttons_16

CURRENT_STAGE="receipt"; python3 - "$OUTPUT_DIR" "$XEX" "$BASEFILE" "$BINARY" \
        "$CODEGEN_MANIFEST" "$GHIDRA_MANIFEST" "$BOUNDARY_CONFIG" \
        "$GENERATED_ADAPTER" "$ADAPTER" "$EXPECTED_XEX_SHA256" \
        "$EXPECTED_BASEFILE_SHA256" "$EXPECTED_CODEGEN_MANIFEST_SHA256" \
        "$EXPECTED_GHIDRA_MANIFEST_SHA256" "$EXPECTED_BOUNDARY_CONFIG_SHA256" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

output_dir = Path(sys.argv[1])
xex = Path(sys.argv[2])
basefile = Path(sys.argv[3])
binary = Path(sys.argv[4])
codegen_manifest = Path(sys.argv[5])
ghidra_manifest = Path(sys.argv[6])
boundary_config = Path(sys.argv[7])
generated_adapter = Path(sys.argv[8])
adapter = Path(sys.argv[9])
expected_xex, expected_basefile = sys.argv[10:12]
expected_codegen, expected_ghidra, expected_boundary = sys.argv[12:15]

def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

def read_json(path: Path) -> dict:
    if path.stat().st_size > 32 * 1024 * 1024:
        raise SystemExit(f"JSON artifact exceeds bound: {path}")
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise SystemExit(f"JSON artifact is not an object: {path}")
    return value

def artifact(path: Path) -> dict[str, object]:
    return {"path": path.name, "sha256": sha256(path),
            "bytes": path.stat().st_size}

if sha256(xex) != expected_xex or sha256(basefile) != expected_basefile:
    raise SystemExit("final identity changed before capsule creation")
if sha256(codegen_manifest) != expected_codegen:
    raise SystemExit("final codegen manifest changed before capsule creation")
if sha256(ghidra_manifest) != expected_ghidra:
    raise SystemExit("final Ghidra manifest changed before capsule creation")
if sha256(boundary_config) != expected_boundary:
    raise SystemExit("final boundary config changed before capsule creation")
if generated_adapter.read_bytes() != adapter.read_bytes():
    raise SystemExit("final generated adapter differs from source adapter")

routes = {}
for route in ("neutral", "buttons_16"):
    report_path = output_dir / f"{route}.report.json"
    trace_path = output_dir / f"{route}.trace"
    stdout_path = output_dir / f"{route}.stdout.log"
    stderr_path = output_dir / f"{route}.stderr.log"
    mapping_path = output_dir / f"{route}.post-resume-map.json"
    report = read_json(report_path)
    mapping = read_json(mapping_path)
    if report.get("schema") != "ac6-demo-frontier-report/v1":
        raise SystemExit(f"{route}: wrong frontier report schema")
    if report.get("target", {}).get("xex_sha256") != expected_xex:
        raise SystemExit(f"{route}: frontier report XEX mismatch")
    if report.get("target", {}).get("module") != "Default.xex":
        raise SystemExit(f"{route}: frontier report module mismatch")
    if report.get("request") != {
        "until": "frontend", "max_ticks": 5600, "backend": "vulkan"
    }:
        raise SystemExit(f"{route}: probe request differs from cycle-1756 contract")
    outcome = report.get("outcome", {})
    milestones = report.get("milestones", {})
    graphics = report.get("graphics", {})
    scheduler = report.get("scheduler", {})
    if outcome.get("kind") != "max_ticks" or outcome.get("completed_ticks") != 5600:
        raise SystemExit(f"{route}: probe did not complete the 5600-tick bound")
    if milestones.get("presents") != 5463:
        raise SystemExit(f"{route}: presentation count differs from cycle-1756 bound")
    if any(milestones.get(key) is not False for key in ("frontend", "mission", "terminal")):
        raise SystemExit(f"{route}: frontend/mission/terminal state is not false")
    if graphics.get("presentation_notifications") != 5463:
        raise SystemExit(f"{route}: graphics presentation count mismatch")
    if {key: scheduler.get(key) for key in ("threads", "blocked", "runnable", "finished")} != {
        "threads": 23, "blocked": 23, "runnable": 0, "finished": 0
    }:
        raise SystemExit(f"{route}: scheduler boundary differs from cycle-1756")
    identity = report.get("identity", {})
    if identity.get("trace_sha256") != sha256(trace_path):
        raise SystemExit(f"{route}: report trace hash does not match captured trace")
    if identity.get("codegen_manifest_sha256") != expected_codegen:
        raise SystemExit(f"{route}: report codegen identity mismatch")
    if identity.get("ghidra_manifest_sha256") != expected_ghidra:
        raise SystemExit(f"{route}: report Ghidra identity mismatch")
    if identity.get("boundary_config_sha256") != expected_boundary:
        raise SystemExit(f"{route}: report boundary identity mismatch")
    if mapping.get("schema") != "ac6-demo-generated-guest-load-map/v1" or \
       mapping.get("mode") != "post_resume_one_shot":
        raise SystemExit(f"{route}: mapper did not emit the post-resume schema")
    if mapping.get("target", {}).get("xex_sha256") != expected_xex or \
       mapping.get("target", {}).get("pal_basefile_sha256") != expected_basefile:
        raise SystemExit(f"{route}: mapper target identity mismatch")
    policy = mapping.get("policy", {})
    if policy.get("exactly_one_instruction_handoff") is not True or \
       policy.get("exactly_one_memory_access") is not True or \
       policy.get("fail_closed") is not True:
        raise SystemExit(f"{route}: mapper policy is not one-shot fail-closed")
    handoff_count = int((output_dir / f"{route}.handoff_count").read_text())
    access_count = int((output_dir / f"{route}.access_count").read_text())
    if handoff_count != 1 or access_count > 1:
        raise SystemExit(f"{route}: post-resume record bound violated")
    routes[route] = {
        "input": "neutral" if route == "neutral" else {
            "tick": 252, "buttons": "0x0010", "lt": 0, "rt": 0,
            "lx": 0, "ly": 0, "rx": 0, "ry": 0, "connected": True,
        },
        "return_code": int((output_dir / f"{route}.return_code").read_text()),
        "handoff_count": handoff_count,
        "access_count": access_count,
        "report": artifact(report_path),
        "trace": artifact(trace_path),
        "stdout": artifact(stdout_path),
        "stderr": artifact(stderr_path),
        "post_resume_mapping": artifact(mapping_path),
        "mapper_stdout": artifact(output_dir / f"{route}.mapper.stdout.log"),
        "mapper_stderr": artifact(output_dir / f"{route}.mapper.stderr.log"),
        "report_summary": {
            "outcome": outcome,
            "milestones": milestones,
            "scheduler": {key: scheduler[key] for key in (
                "threads", "blocked", "runnable", "finished")},
        },
        "post_resume": {
            "handoff": mapping.get("handoff"),
            "access": mapping.get("access"),
        },
    }

stable_keys = ("outcome", "milestones", "graphics", "scheduler")
reports = [read_json(output_dir / f"{route}.report.json") for route in routes]
stable_equal = [key for key in stable_keys
                if reports[0].get(key) == reports[1].get(key)]
if stable_equal != list(stable_keys):
    raise SystemExit(f"A/B report stable subtrees differ: {stable_equal}")
neutral_handoff = routes["neutral"]["post_resume"]["handoff"]
buttons_handoff = routes["buttons_16"]["post_resume"]["handoff"]
if neutral_handoff != buttons_handoff:
    raise SystemExit("A/B post-resume handoff boundary differs")

capsule = {
    "schema": "ac6-demo-post-resume-ab-one-shot/v1",
    "target": {
        "id": "ac6-demo-xbox360-pal",
        "module": "Default.xex",
        "xex_sha256": expected_xex,
        "ghidra_project": "ace-combat-6-demo",
        "basefile_sha256": expected_basefile,
        "architecture": "Xenon big-endian / Xenos",
    },
    "scope": {
        "project": "ace-combat-6-demo",
        "ghidra_project": "ace-combat-6-demo",
        "backend": "vulkan",
        "max_ticks": 5600,
        "fresh_process_per_run": True,
        "input_at": "252,16,0,0,0,0,0,0,1",
        "environment": [
            "SDL_AUDIODRIVER=dummy",
            "AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1",
            "AC6_DEMO_EXPERIMENTAL_XMA_KICK=1",
            "AC6_DEMO_WATCH_POST_RESUME_ACCESS=1",
            "Xvfb -displayfd (owned per route)",
        ],
        "instrumentation_default": False,
        "output_layout": "sha256/<receipt_sha256>",
        "binary": artifact(binary),
        "codegen_manifest": artifact(codegen_manifest),
        "ghidra_manifest": artifact(ghidra_manifest),
        "boundary_config": artifact(boundary_config),
        "generated_adapter": artifact(generated_adapter),
        "source_adapter": artifact(adapter),
    },
    "runs": routes,
    "comparison": {
        "report_subtrees_json_object_equal": stable_equal,
        "report_sha256": {
            "neutral": routes["neutral"]["report"]["sha256"],
            "buttons_16": routes["buttons_16"]["report"]["sha256"],
        },
        "trace_sha256": {
            "neutral": routes["neutral"]["trace"]["sha256"],
            "buttons_16": routes["buttons_16"]["trace"]["sha256"],
        },
        "stderr_sha256": {
            "neutral": routes["neutral"]["stderr"]["sha256"],
            "buttons_16": routes["buttons_16"]["stderr"]["sha256"],
        },
        "stderr_byte_equal": routes["neutral"]["stderr"]["sha256"] == routes["buttons_16"]["stderr"]["sha256"],
        "trace_byte_equal": routes["neutral"]["trace"]["sha256"] == routes["buttons_16"]["trace"]["sha256"],
        "post_resume_handoff_equal": True,
        "post_resume_accesses_bounded": all(route["access_count"] <= 1 for route in routes.values()),
    },
    "classification": {
        "demo-qualified": [
            "two fresh PAL demo processes completed the cycle-1756 5600-tick Vulkan bound",
            "exactly one qualified post-resume handoff was observed per route",
            "the existing generated-source mapper joined each one-shot access fail-closed",
        ],
        "demo-observed": [
            "route-specific post-resume access records and mapped PAL instruction bytes",
            "A/B report, trace, stdout and stderr hashes",
        ],
        "unknown": [
            "frontend, mission and terminal semantics",
            "native parity and guest-owned presentation/readback causality",
        ],
    },
    "policy": {
        "retail_or_historical_evidence_used": False,
        "xenia_used": False,
        "generated_cpp_modified": False,
        "ghidra_modified": False,
        "frontend_promoted": False,
        "native_parity_promoted": False,
        "fail_closed": True,
    },
}
output_path = output_dir / "receipt.json"
canonical = json.dumps(capsule, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n"
output_path.write_bytes(canonical.encode("utf-8"))
PY

trees_identical() {
  local left="$1"
  local right="$2"
  diff -qr --no-dereference -- "$left" "$right" >/dev/null 2>&1
}

publish_transaction() {
  local receipt="$STAGING/receipt.json"
  local receipt_sha256
  local publish_root="$OUTPUT_ROOT/sha256"
  local destination
  require_bounded_file "$receipt" "$MAX_REPORT_BYTES"
  receipt_sha256="$(sha256_file "$receipt")"
  [[ "$receipt_sha256" =~ ^[0-9a-f]{64}$ ]] ||
    die "receipt SHA-256 is malformed"
  destination="$publish_root/$receipt_sha256"

  # The staging directory and destination share a device, so this final move
  # is a directory rename. No output path is touched before this point.
  if [[ -e "$OUTPUT_ROOT" || -L "$OUTPUT_ROOT" ]]; then
    [[ -d "$OUTPUT_ROOT" && ! -L "$OUTPUT_ROOT" ]] ||
      die "output root is not a regular directory: $OUTPUT_ROOT"
  fi
  mkdir -p -- "$publish_root"
  [[ -d "$publish_root" && ! -L "$publish_root" ]] ||
    die "publication root is not a regular directory: $publish_root"

  if [[ -e "$destination" || -L "$destination" ]]; then
    [[ -d "$destination" && ! -L "$destination" ]] ||
      die "receipt digest collision is not a directory: $destination"
    if trees_identical "$STAGING" "$destination"; then
      rm -rf -- "$STAGING"
      printf 'post-resume A/B capsule (existing identical receipt): %s/receipt.json\n' \
        "$destination"
      return 0
    fi
    die "receipt digest collision has different content: $destination"
  fi

  if mv -T --no-copy -- "$STAGING" "$destination"; then
    printf 'post-resume A/B capsule: %s/receipt.json\n' "$destination"
    return 0
  fi
  # A concurrent publisher may have won the same digest. Accept only a
  # byte-for-byte directory match; never overwrite or merge its evidence.
  if [[ -d "$destination" && ! -L "$destination" ]] && \
     trees_identical "$STAGING" "$destination"; then
    rm -rf -- "$STAGING"
    printf 'post-resume A/B capsule (existing identical receipt): %s/receipt.json\n' \
      "$destination"
    return 0
  fi
  die "atomic receipt publication failed: $destination"
}

publish_transaction
