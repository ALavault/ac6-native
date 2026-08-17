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

XEX="$WORKSPACE/demo-game-file/extracted/stfs-root/Default.xex"
BUILD="$PROJECT/build-codegen-on"
BINARY="$BUILD/ac6-demo-recomp"
CODEGEN="$BUILD/codegen"
BASEFILE="$CODEGEN/xex-basefile.bin"
GENERATED="$CODEGEN/generated"
MANIFEST="$CODEGEN/manifest.json"
STORE_SEED="$WORKSPACE/.build/ac6-demo-atomic-start-vulkan-store"
MAPPER="$PROJECT/tools/map_xam_return_chain.py"
OUTPUT_ROOT="$WORKSPACE/analysis/demo/ac6-demo-xam-return-chain-ab"

MAX_REPORT_BYTES=$((32 * 1024 * 1024))
MAX_TRACE_BYTES=$((128 * 1024 * 1024))
MAX_LOG_BYTES=$((32 * 1024 * 1024))
MAX_MAP_BYTES=$((16 * 1024 * 1024))
MAX_RETURN_BYTES=32
PROBE_TIMEOUT_SECONDS=900

RUN_TMP="$(mktemp -d "$TMPDIR/ac6-xam-return-chain.XXXXXX")"
CAPSULE_PUBLISHED=0
RUN_REASON=""
RUN_STATUS="failure"
export RUN_REASON

sha256_file() { sha256sum -- "$1" | awk '{print $1}'; }

require_file() {
  local path="$1"
  [[ -f "$path" && ! -L "$path" ]] || {
    RUN_REASON="missing or symlinked file: $path"
    return 1
  }
}

bounded_file() {
  local path="$1" limit="$2" size
  require_file "$path" || return 1
  size="$(stat -c '%s' -- "$path")"
  (( size <= limit )) || {
    RUN_REASON="capture exceeds quota: $path ($size > $limit)"
    return 1
  }
}

fail_run() {
  RUN_REASON="$1"
  return 1
}

publish_capsule() {
  local status="$1" reason="$2" stage digest destination route label source limit size sha
  stage="$RUN_TMP/publish-$status"
  mkdir -p -- "$stage/artifacts"
  for route in neutral buttons16; do
    mkdir -p -- "$stage/artifacts/$route"
    for label in "$route.trace" "$route.report.json" "$route.stdout.log" \
                 "$route.stderr.log" "$route.map.json" return_code; do
      source="$RUN_TMP/$route/$label"
      case "$label" in
        "$route.trace") limit=$MAX_TRACE_BYTES ;;
        "$route.report.json") limit=$MAX_REPORT_BYTES ;;
        "$route.map.json") limit=$MAX_MAP_BYTES ;;
        return_code) limit=$MAX_RETURN_BYTES ;;
        *) limit=$MAX_LOG_BYTES ;;
      esac
      if [[ -f "$source" && ! -L "$source" ]]; then
        size="$(stat -c '%s' -- "$source")"
        sha="$(sha256_file "$source")"
        if (( size <= limit )); then
          cp -- "$source" "$stage/artifacts/$route/$label"
          printf '%s\t%s\tpresent\t%s\t%s\t%s\n' \
            "$route" "$label" "$size" "$sha" "$limit" >>"$stage/index.tsv"
        else
          printf '%s\t%s\tomitted_quota\t%s\t%s\t%s\n' \
            "$route" "$label" "$size" "$sha" "$limit" >>"$stage/index.tsv"
        fi
      else
        printf '%s\t%s\tmissing\t0\t\t%s\n' \
          "$route" "$label" "$limit" >>"$stage/index.tsv"
      fi
    done
  done
  python3 - "$stage/receipt.json" "$stage/index.tsv" "$status" "$reason" \
    "$EXPECTED_XEX_SHA256" "$EXPECTED_BASEFILE_SHA256" "$BINARY" "$MANIFEST" \
    "$MAPPER" "$RUN_STATUS" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

out, index, status, reason, xex_sha, base_sha, binary, manifest, mapper, run_status = sys.argv[1:]
artifacts = {}
for line in Path(index).read_text(encoding="utf-8").splitlines():
    route, label, state, size, digest, limit = line.split("\t")
    artifacts.setdefault(route, {})[label] = {
        "status": state, "bytes": int(size), "sha256": digest or None,
        "quota_bytes": int(limit),
        "path": f"artifacts/{route}/{label}" if state == "present" else None,
    }

def digest(path: str):
    try:
        data = Path(path).read_bytes()
    except OSError:
        return None
    return hashlib.sha256(data).hexdigest()

document = {
    "schema": "ac6-demo-xam-return-chain-ab/v1",
    "status": status,
    "reason": reason,
    "boundary": "xam_return_chain",
    "target": {"id": "ac6-demo-xbox360-pal", "module": "Default.xex",
               "xex_sha256": xex_sha, "pal_basefile_sha256": base_sha},
    "inputs": {
        "binary": {"path": binary, "sha256": digest(binary)},
        "codegen_manifest": {"path": manifest, "sha256": digest(manifest)},
        "mapper": {"path": mapper, "sha256": digest(mapper)},
    },
    "config": {
        "routes": ["neutral", "buttons16"], "tick": 252, "max_ticks": 5600,
        "max_accesses": 64,
        "backend": "vulkan", "audio_driver": "dummy",
        "xma": "qualified runtime flags",
        "xvfb_lifecycle": "single owned supervisor reused by both fresh probes",
    },
    "routes": artifacts,
    "policy": {"content_addressed": True, "transactional": True,
               "quotas_and_hashes": True, "current_file_written": False,
               "frontend_claim_written": False, "fail_closed": True},
}
Path(out).write_text(json.dumps(document, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")
PY
  bounded_file "$stage/receipt.json" "$MAX_REPORT_BYTES" || return 1
  digest="$(sha256_file "$stage/receipt.json")"
  destination="$OUTPUT_ROOT/sha256/$digest"
  if [[ -e "$OUTPUT_ROOT" || -L "$OUTPUT_ROOT" ]]; then
    [[ -d "$OUTPUT_ROOT" && ! -L "$OUTPUT_ROOT" ]] || return 1
  fi
  mkdir -p -- "$OUTPUT_ROOT/sha256"
  if [[ -e "$destination" || -L "$destination" ]]; then
    [[ -d "$destination" && ! -L "$destination" ]] || return 1
    diff -qr --no-dereference -- "$stage" "$destination" >/dev/null || return 1
  else
    mv -T -- "$stage" "$destination"
  fi
  CAPSULE_PUBLISHED=1
  printf 'xam return-chain %s capsule: %s\n' "$status" "$destination"
}

finish() {
  local rc=$?
  set +e
  if (( CAPSULE_PUBLISHED == 0 )); then
    publish_capsule failure "${RUN_REASON:-runner exited with status $rc}" >/dev/null 2>&1 || true
  fi
  rm -rf -- "$RUN_TMP"
  exit "$rc"
}
trap finish EXIT

require_file "$XEX"
require_file "$BINARY"
require_file "$BASEFILE"
require_file "$MANIFEST"
[[ -x "$BINARY" ]] || { fail_run "runtime binary is not executable"; exit 1; }
[[ "$(sha256_file "$XEX")" == "$EXPECTED_XEX_SHA256" ]] || {
  fail_run "XEX SHA-256 mismatch"; exit 1;
}
[[ "$(sha256_file "$BASEFILE")" == "$EXPECTED_BASEFILE_SHA256" ]] || {
  fail_run "PAL basefile SHA-256 mismatch"; exit 1;
}
[[ -d "$GENERATED" && ! -L "$GENERATED" ]] || {
  fail_run "generated directory is missing or symlinked"; exit 1;
}
require_file "$STORE_SEED/.ac6-demo-store"
require_file "$STORE_SEED/Default.xex"
cmp -- "$STORE_SEED/Default.xex" "$XEX" || {
  fail_run "store seed XEX differs from qualified XEX"; exit 1;
}
grep -Fxq "$EXPECTED_XEX_SHA256" "$STORE_SEED/.ac6-demo-store" || {
  fail_run "store seed marker is not qualified"; exit 1;
}

# One owned Xvfb supervisor is reused by two fresh probe processes.  The
# process identity (start time and process group) is checked before and after
# each probe, so an unrelated X server can never be signalled or reused.
python3 - "$RUN_TMP" "$BINARY" "$STORE_SEED" "$MAX_TRACE_BYTES" \
  "$MAX_REPORT_BYTES" "$MAX_LOG_BYTES" "$PROBE_TIMEOUT_SECONDS" <<'PY'
import os
import select
import shutil
import subprocess
import sys
from pathlib import Path

run_root, binary, seed, trace_limit, report_limit, log_limit, timeout = sys.argv[1:]
run_root = Path(run_root)
seed = Path(seed)

def proc_identity(pid: int):
    fields = Path(f"/proc/{pid}/stat").read_text(encoding="ascii").split()
    return int(fields[21]), int(fields[4])

def bounded(path: Path, limit: int):
    if path.exists() and path.stat().st_size > limit:
        raise RuntimeError(f"capture quota exceeded: {path}")

xvfb = None
try:
    xvfb = subprocess.Popen(
        ["Xvfb", "-displayfd", "1", "-screen", "0", "1280x720x24",
         "-nolisten", "tcp"], stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, text=True, start_new_session=True)
    ready, _, _ = select.select([xvfb.stdout], [], [], 10) if xvfb.stdout else ([], [], [])
    display_line = xvfb.stdout.readline().strip() if ready else ""
    if not display_line.isdigit():
        raise RuntimeError("owned Xvfb did not publish a display")
    display = ":" + display_line
    identity = proc_identity(xvfb.pid)
    if identity[1] != xvfb.pid:
        raise RuntimeError("Xvfb process-group identity is not owned")
    for mode, buttons in (("neutral", "0"), ("buttons16", "16")):
        route = run_root / mode
        route.mkdir()
        store = route / "store"
        shutil.copytree(seed, store, symlinks=False)
        trace = route / f"{mode}.trace"
        report = route / f"{mode}.report.json"
        stdout = route / f"{mode}.stdout.log"
        stderr = route / f"{mode}.stderr.log"
        env = {key: value for key, value in os.environ.items()
               if not key.startswith("AC6_DEMO_")}
        env.update({
            "DISPLAY": display, "SDL_AUDIODRIVER": "dummy",
            "AC6_DEMO_WATCH_XAM_RETURN_CHAIN": "1",
            "AC6_DEMO_EXPERIMENTAL_XMA_CREATE": "1",
            "AC6_DEMO_EXPERIMENTAL_XMA_KICK": "1",
            "AC6_DEMO_WATCH_XMA_SLOT": "1",
            "AC6_DEMO_INPUT_MODE": mode, "AC6_DEMO_INPUT_TICK": "252",
            "AC6_DEMO_MAX_TICKS": "5600",
        })
        input_at = f"252,{buttons},0,0,0,0,0,0,1"
        command = [binary, "probe", "--store", str(store), "--until", "frontend",
                   "--max-ticks", "5600", "--trace", str(trace),
                   "--report", str(report), "--backend", "vulkan",
                   "--input-at", input_at]
        with stdout.open("w", encoding="utf-8") as out, stderr.open(
                "w", encoding="utf-8") as err:
            completed = subprocess.run(command, env=env, stdout=out, stderr=err,
                                       timeout=int(timeout))
        (route / "return_code").write_text(str(completed.returncode) + "\n",
                                            encoding="ascii")
        for path, limit in ((trace, int(trace_limit)), (report, int(report_limit)),
                            (stdout, int(log_limit)), (stderr, int(log_limit))):
            bounded(path, limit)
        if proc_identity(xvfb.pid) != identity:
            raise RuntimeError("Xvfb identity changed during probe reuse")
finally:
    if xvfb is not None:
        try:
            if xvfb.poll() is None:
                xvfb.terminate()
                xvfb.wait(timeout=5)
            if xvfb.poll() is None:
                xvfb.kill()
                xvfb.wait(timeout=2)
        finally:
            if xvfb.stdout is not None:
                xvfb.stdout.close()
            if xvfb.stderr is not None:
                xvfb.stderr.close()
PY

for route in neutral buttons16; do
  trace="$RUN_TMP/$route/$route.trace"
  stderr="$RUN_TMP/$route/$route.stderr.log"
  report="$RUN_TMP/$route/$route.report.json"
  bounded_file "$trace" "$MAX_TRACE_BYTES"
  bounded_file "$stderr" "$MAX_LOG_BYTES"
  bounded_file "$report" "$MAX_REPORT_BYTES"
  rc="$(<"$RUN_TMP/$route/return_code")"
  [[ "$rc" == 4 ]] || { fail_run "$route probe did not finish at tick bound (rc=$rc)"; exit 1; }
  arm_count="$(grep -a -c '^AC6_XAM_RETURN_CHAIN_ARM ' "$stderr" || true)"
  access_count="$(grep -a -c '^AC6_XAM_RETURN_CHAIN_ACCESS ' "$stderr" || true)"
  refused_count="$(grep -a -c '^AC6_XAM_RETURN_CHAIN_ACCESS_REFUSED ' "$stderr" || true)"
  stop_count="$(grep -a -c '^AC6_XAM_RETURN_CHAIN_STOP ' "$stderr" || true)"
  [[ "$arm_count" == 1 ]] || { fail_run "$route did not emit exactly one XAM arm"; exit 1; }
  (( access_count > 0 && access_count <= 64 )) || {
    fail_run "$route emitted an empty or over-bounded XAM chain"; exit 1;
  }
  [[ "$refused_count" == 0 ]] || { fail_run "$route emitted an XAM refusal"; exit 1; }
  [[ "$stop_count" == 1 ]] || { fail_run "$route did not close exactly once"; exit 1; }
  grep -Eq 'state16=[0-9A-Fa-f]{32}$' "$stderr" || {
    fail_run "$route arm did not carry a complete state16 payload"; exit 1;
  }
  python3 "$MAPPER" --trace "$stderr" --basefile "$BASEFILE" --xex "$XEX" \
    --generated-dir "$GENERATED" --manifest "$MANIFEST" \
    --json "$RUN_TMP/$route/$route.map.json" || {
      fail_run "$route mapper refused the trace"; exit 1;
    }
  bounded_file "$RUN_TMP/$route/$route.map.json" "$MAX_MAP_BYTES"
done

RUN_STATUS="success"
publish_capsule success "qualified XAM return-chain A/B completed"
