#!/usr/bin/env bash
set -euo pipefail

export TMPDIR=/fastdata/lavaulta/tmp
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
RUNNER="$ROOT/recompilation/ace-combat-6-demo/tools/run_post_resume_ab_one_shot.sh"
TEST_TMP="$(mktemp -d "$TMPDIR/ac6-xvfb-test.XXXXXX")"
trap 'rm -rf -- "$TEST_TMP"' EXIT
mkdir -- "$TEST_TMP/bin" "$TEST_TMP/run"

# The test sources the production function and exercises its real process
# ownership/cleanup paths; only Xvfb and the guest probe are deterministic fakes.
awk '/^run_capped_probe\(\) \{/{on=1} /^run_route\(\) \{/{on=0} on' \
  "$RUNNER" >"$TEST_TMP/helper.sh"

cat >"$TEST_TMP/bin/Xvfb" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
fd=3
while (($#)); do
  if [[ "$1" == -displayfd ]]; then fd="$2"; shift 2; else shift; fi
done
if [[ -n "${FAKE_XVFB_PID_FILE:-}" ]]; then
  printf '%s\n' "$$" >"$FAKE_XVFB_PID_FILE"
fi
case "${FAKE_XVFB_MODE:-ready}" in
  fail) exit 77 ;;
  no-display) ;;
  ready) printf '97\n' >&"$fd" ;;
  colon-display) printf ':97\n' >&"$fd" ;;
  empty-display) printf '\n' >&"$fd" ;;
  nonnum-display) printf 'not-a-display\n' >&"$fd" ;;
  multiline-display) printf '97\n98\n' >&"$fd" ;;
  invalid-display) printf 'not-a-display\n' >&"$fd" ;;
  *) exit 78 ;;
esac
trap 'if [[ "${FAKE_XVFB_TERM_IGNORED:-0}" == 1 ]]; then :; else exit 0; fi' TERM
while :; do sleep 0.05; done
SH
chmod +x "$TEST_TMP/bin/Xvfb"

cat >"$TEST_TMP/bin/probe" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
case "${FAKE_PROBE_MODE:-rc}" in
  rc) exit "${FAKE_PROBE_RC:-4}" ;;
  timeout) sleep 30 ;;
  *) exit 79 ;;
esac
SH
chmod +x "$TEST_TMP/bin/probe"

fail() { printf 'post-resume Xvfb lifecycle: %s\n' "$*" >&2; exit 1; }

assert_no_orphans() {
  local case_tmp="$1" pid state
  if [[ -s "$case_tmp/fake.pid" ]]; then
    pid="$(<"$case_tmp/fake.pid")"
    if [[ -r "/proc/$pid/stat" ]]; then
      state="$(awk '{print $3}' "/proc/$pid/stat")"
      [[ "$state" == Z ]] || fail "orphan Xvfb PID $pid ($state) in $case_tmp"
      fail "unreaped Xvfb zombie PID $pid in $case_tmp"
    fi
  fi
  [[ -z "$(find "$case_tmp" -mindepth 1 -type d -print -quit)" ]] ||
    fail "orphan Xvfb staging directory in $case_tmp"
}

proc_identity() {
  local pid="$1"
  [[ -r "/proc/$pid/stat" ]] || return 1
  awk '{print $22 ":" $5 ":" $3}' "/proc/$pid/stat"
}

identity_matches() {
  local pid="$1" start="$2" pgid="$3" current state current_start current_pgid
  current="$(proc_identity "$pid")" || return 1
  IFS=: read -r current_start current_pgid state <<<"$current"
  [[ "$state" != Z && "$current_start" == "$start" && "$current_pgid" == "$pgid" && "$pgid" == "$pid" ]]
}

start_real_xvfb() {
  local dir="$1"
  : >"$dir/display"
  setsid /usr/bin/Xvfb -displayfd 3 -screen 0 640x480x24 -nolisten tcp \
    3>"$dir/display" >"$dir/stdout" 2>"$dir/stderr" &
  STARTED_PID=$!
}

wait_displayfd() {
  local file="$1"
  for _ in {1..100}; do
    [[ -s "$file" ]] && return 0
    sleep 0.05
  done
  return 1
}

parse_displayfd() {
  local file="$1" parsed
  parsed="$(python3 - "$file" <<'PY'
import json
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
print(f"{number}\t{json.dumps(raw)}")
PY
)" || return 1
  DISPLAY_NUMBER="${parsed%%$'\t'*}"
  DISPLAY_RAW_REPR="${parsed#*$'\t'}"
  DISPLAY_NORMALIZED=":$DISPLAY_NUMBER"
}

cleanup_real_owned() {
  local pid="$1" start="$2" pgid="$3" state
  [[ -n "$pid" ]] || return 0
  if identity_matches "$pid" "$start" "$pgid"; then
    kill -TERM -- "-$pgid" 2>/dev/null || true
  fi
  for _ in {1..40}; do
    if [[ ! -r "/proc/$pid/stat" ]]; then break; fi
    state="$(awk '{print $3}' "/proc/$pid/stat" 2>/dev/null || true)"
    [[ "$state" == Z ]] && break
    sleep 0.05
  done
  if [[ -r "/proc/$pid/stat" ]] && identity_matches "$pid" "$start" "$pgid"; then
    kill -KILL -- "-$pgid" 2>/dev/null || true
  fi
  for _ in {1..40}; do
    if [[ ! -r "/proc/$pid/stat" ]]; then break; fi
    state="$(awk '{print $3}' "/proc/$pid/stat" 2>/dev/null || true)"
    [[ "$state" == Z ]] && break
    sleep 0.05
  done
  if [[ -r "/proc/$pid/stat" ]]; then
    state="$(awk '{print $3}' "/proc/$pid/stat" 2>/dev/null || true)"
    [[ "$state" == Z ]] && wait "$pid" 2>/dev/null || true
  fi
}

smoke_real_xvfb() {
  local smoke_dir="$TEST_TMP/smoke" foreign_dir="$TEST_TMP/smoke-foreign"
  local foreign_pid='' owned_pid='' foreign_start='' foreign_pgid=''
  local owned_start='' owned_pgid=''
  local foreign_display_normalized=
  local foreign_before=0 foreign_after=0 smoke_status=0
  mkdir -- "$smoke_dir" "$foreign_dir"

  start_real_xvfb "$foreign_dir"
  foreign_pid="$STARTED_PID"
  if wait_displayfd "$foreign_dir/display" && parse_displayfd "$foreign_dir/display"; then
    foreign_display_normalized="$DISPLAY_NORMALIZED"
    IFS=: read -r foreign_start foreign_pgid _ < <(proc_identity "$foreign_pid") || smoke_status=1
  else
    smoke_status=1
  fi

  if (( smoke_status == 0 )); then
    start_real_xvfb "$smoke_dir"
    owned_pid="$STARTED_PID"
    if ! wait_displayfd "$smoke_dir/display"; then
      smoke_status=1
    elif ! parse_displayfd "$smoke_dir/display"; then
      smoke_status=1
    else
      IFS=: read -r owned_start owned_pgid _ < <(proc_identity "$owned_pid") || smoke_status=1
      for _ in {1..40}; do
        if xdpyinfo -display "$DISPLAY_NORMALIZED" >/dev/null 2>&1; then break; fi
        sleep 0.05
      done
      xdpyinfo -display "$DISPLAY_NORMALIZED" >/dev/null 2>&1 || smoke_status=1
      kill -0 "$foreign_pid" 2>/dev/null && foreign_before=1
      cleanup_real_owned "$owned_pid" "$owned_start" "$owned_pgid"
      kill -0 "$foreign_pid" 2>/dev/null && foreign_after=1
      [[ "$foreign_before" == 1 && "$foreign_after" == 1 ]] || smoke_status=1
      xdpyinfo -display "$foreign_display_normalized" >/dev/null 2>&1 || smoke_status=1
      [[ ! -e "/proc/$owned_pid/stat" ]] || smoke_status=1
    fi
  fi

  # Both servers were started by this test; cleanup is still identity-owned.
  if [[ -n "$owned_pid" ]]; then cleanup_real_owned "$owned_pid" "$owned_start" "$owned_pgid"; fi
  if [[ -n "$foreign_pid" ]]; then cleanup_real_owned "$foreign_pid" "$foreign_start" "$foreign_pgid"; fi
  if (( smoke_status != 0 )); then
    fail "real Xvfb smoke failed"
  fi
  printf 'real Xvfb smoke: raw=%s normalized=%s foreign_pid=%s before=%s after=%s\n' \
    "$DISPLAY_RAW_REPR" "$DISPLAY_NORMALIZED" "$foreign_pid" "$foreign_before" "$foreign_after"
}

run_case() {
  local name="$1" expected_rc="$2" mode="$3" probe_mode="$4" term_ignored="$5"
  local identity_mode="${6:-}" cleanup_failure="${7:-0}" expected_failure="${8:-}"
  local case_tmp="$TEST_TMP/run/$name" got caller_rc failure_text
  mkdir -- "$case_tmp"
  set +e
  /usr/bin/env \
    PATH="$TEST_TMP/bin:$PATH" \
    RUN_TMP="$case_tmp" \
    CAPTURE_FILE_MAX_BYTES=1024 \
    PROBE_TIMEOUT_SECONDS=1 \
    XVFB_START_TIMEOUT_SECONDS=1 \
    FAKE_XVFB_MODE="$mode" \
    FAKE_XVFB_TERM_IGNORED="$term_ignored" \
    FAKE_XVFB_PID_FILE="$case_tmp/fake.pid" \
    FAKE_PROBE_MODE="$probe_mode" \
    FAKE_PROBE_RC="$expected_rc" \
    AC6_TEST_XVFB_IDENTITY_MODE="$identity_mode" \
    AC6_TEST_XVFB_CLEANUP_FAILURE="$cleanup_failure" \
    bash -c '
      set -euo pipefail
      RUN_TMP="$1"
      CAPTURE_FILE_MAX_BYTES=1024
      PROBE_TIMEOUT_SECONDS=1
      XVFB_START_TIMEOUT_SECONDS=1
      RUNNER_AUX_FAILURE=""
      source "$2"
      if run_capped_probe "$RUN_TMP/return_code" "$3"; then rc=0; else rc=$?; fi
      printf "%s\n" "$rc" >"$RUN_TMP/caller_rc"
      printf "%s\n" "${RUNNER_AUX_FAILURE:-}" >"$RUN_TMP/failure"
      exit "$rc"
    ' _ "$case_tmp" "$TEST_TMP/helper.sh" "$TEST_TMP/bin/probe"
  got=$?
  set -e
  [[ "$got" == "$expected_rc" ]] || fail "$name: rc $got != $expected_rc"
  caller_rc="$(<"$case_tmp/caller_rc")"
  [[ "$caller_rc" == "$expected_rc" ]] || fail "$name: caller rc not conserved"
  failure_text="$(<"$case_tmp/failure")"
  if [[ -n "$expected_failure" ]]; then
    [[ "$failure_text" == "$expected_failure" ]] ||
      fail "$name: failure '$failure_text' != '$expected_failure'"
  elif [[ "$cleanup_failure" == 1 ]]; then
    [[ "$failure_text" == "xvfb cleanup failed" ]] ||
      fail "$name: cleanup failure was not structured"
  else
    [[ -z "$failure_text" ]] || fail "$name: unexpected auxiliary failure: $failure_text"
  fi
  assert_no_orphans "$case_tmp"
}

run_case rc4 4 ready rc 0
# ctest (and any parent holding a log there) leaves fd 3 open in the test;
# the supervisor must still hand Xvfb the display file on exactly fd 3.
exec 3>"$TEST_TMP/inherited-fd3"
run_case rc4-inherited-fd3 4 ready rc 0
exec 3>&-
run_case rc3 3 ready rc 0
run_case probe-timeout 124 ready timeout 0
run_case start-immediate-fail 125 fail rc 0 '' 0 'xvfb start failed'
run_case displayfd-absent-term-kill 125 no-display rc 1 '' 0 'xvfb start failed'
run_case displayfd-colon-rejected 125 colon-display rc 0 '' 0 'xvfb display allocation failed'
run_case displayfd-empty-rejected 125 empty-display rc 0 '' 0 'xvfb display allocation failed'
run_case displayfd-nonnumeric-rejected 125 nonnum-display rc 0 '' 0 'xvfb display allocation failed'
run_case displayfd-multiline-rejected 125 multiline-display rc 0 '' 0 'xvfb display allocation failed'
run_case ready-term-kill 124 ready timeout 1

sleep 30 & foreign_pid=$!
run_case proc-identity-mismatch 4 ready rc 1 mismatch
kill -0 "$foreign_pid" 2>/dev/null || fail 'foreign process was signalled on identity mismatch'
kill "$foreign_pid" 2>/dev/null || true
wait "$foreign_pid" 2>/dev/null || true

sleep 30 & foreign_pid=$!
run_case proc-identity-absent 4 ready rc 1 absent
kill -0 "$foreign_pid" 2>/dev/null || fail 'foreign process was signalled on identity absence'
kill "$foreign_pid" 2>/dev/null || true
wait "$foreign_pid" 2>/dev/null || true

run_case cleanup-failure-preserves-rc 4 ready rc 0 '' 1

smoke_real_xvfb

printf 'post-resume Xvfb lifecycle: 14 real cases + 1 real-Xvfb smoke PASS\n'
