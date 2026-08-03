#!/usr/bin/env bash
# Launch ac6recomp headless on a private X display, optionally drive it with a
# scripted key sequence, and capture frames on a schedule.
#
# The harness that produced every runtime measurement in cycles 3xx-452 lived
# outside this repository, in a scratch directory, hardcoding one worktree path.
# Results taken with it were not reproducible by anyone who did not also have
# that directory. This is that harness, in the tree, parameterised.
#
# Usage:
#   tools/ac6-run.sh --out DIR --duration 90 [options] [-- extra ac6recomp args]
#
#   --out DIR         output directory (created, PNGs cleared)
#   --duration SEC    how long to run the game
#   --keys SEQ        comma-separated  AT:KEY:HOLD  triples, seconds:
#                       --keys "20:Left:0.6,25:space:0.6"
#                     KEY is an X keysym. The MnK bindings that matter:
#                       A = space   B = shift   Start = Escape
#                       D-pad = Up / Down / Left / Right
#                     HOLD is critical and 0.1 is the right value. A bare
#                     `xdotool key` presses and releases inside one frame and
#                     the guest never sees it -- but a long hold is just as
#                     wrong: the guest's own pad object reports delay=480ms
#                     interval=96ms, so a 0.6-0.8s hold fires the initial press
#                     AND several auto-repeats. On a two-option dialog that
#                     moves the highlight there and back and nets zero visible
#                     change, which reads exactly like a dead button. Several
#                     Right presses measured 1.0 band delta for this reason
#                     while others measured 129. At 0.1s each press produces
#                     exactly one edge, and Right measures 134.
#   --capture-at SEC  comma-separated capture times; default is every 15s
#   --display :NN     X display to use (default :77)
#   --binary PATH     ac6recomp to run (default build-rt/ac6recomp)
#   --screen WxH      Xvfb screen size (default 1280x720)
#   --clock MODE      "wall" (default) or "present". With "present" the numbers
#                     in --keys and --capture-at are GUEST FRAMES, counted from
#                     the runtime's own per-frame PRESENT lines, instead of
#                     seconds. Use this whenever the build's speed is not the
#                     stock 60 FPS -- an instrumented build runs at 9-32 FPS, so
#                     a wall-clock press aimed at the dialog lands in the
#                     cutscene instead and the run looks like the guest ignored
#                     it.
#   --then-keys SEQ   after --wait-for matches, apply delay:key:hold events.
#                     Use delay:CAPTURE to take an observation without input.
#   --step-file PATH  after --wait-for/--then-keys, execute a tab-separated
#                     state-driven recipe. Operations are:
#                       wait<TAB>REGEX<TAB>TIMEOUT
#                       wait-pulse<TAB>REGEX<TAB>KEYSYM
#                       key<TAB>KEYSYM<TAB>HOLD
#                       present<TAB>DELTA<TAB>TIMEOUT
#                       sleep<TAB>SECONDS
#                       capture<TAB>LABEL
#   --wait-pulse KEY[+KEY...]:HOLD:INTERVAL
#                     while waiting for --wait-for, inject this key every
#                     INTERVAL seconds. This is intended for Start during the
#                     variable-length intro; a + sequence injects each named
#                     key once per pulse and stops as soon as the guest log
#                     names the requested state.
#   --startup-present-min N
#                     require N guest presentations before driving input
#                     (default 30; 0 disables the startup health gate)
#   --startup-timeout SEC
#                     startup health-gate timeout (default 30)
#
# Two things about this setup are load-bearing and neither is obvious.
#
# The screen is 1280x720 because that is the game's output size, and captures
# are of the X root window. At that size root coordinates and game coordinates
# coincide, so the button band at y 590-660 is the band tools/ac6-band.py
# measures. On a larger root the game occupies a corner, the band lands on
# empty desktop, and every reading is of nothing at all.
#
# The attract loop freezes without input (measured, cycle 335). Start must be
# pressed to leave the title, and until it is, no menu exists and no other key
# does anything -- a Left press into the title screen produces a null result
# that looks exactly like a broken confirm button. Reaching the YES/NO dialog
# takes Start and then A; see --keys in the examples below.
#
#   # reach the dialog, then test that navigation moves the highlight
#   tools/ac6-run.sh --out /tmp/nav --duration 120 \
#       --keys "35:Escape:0.6,42:space:0.6,60:Left:0.6" \
#       --capture-at "34,40,48,55,65"
#
# Everything is timeout-guarded: a hung capture must not stall the experiment.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

OUT=""; DUR=""; KEYS=""; CAPTURE_AT=""; DISP=":77"; SCREEN="1280x720"
BIN="$REPO/build-rt/ac6recomp"
CLOCK="wall"
WAITFOR=""
THENKEYS=""
WAITPULSE=""
STEPFILE=""
WAIT_STALL_TIMEOUT=0
STARTUP_PRESENT_MIN=30
STARTUP_TIMEOUT=30

while [ $# -gt 0 ]; do
  case "$1" in
    --out)        OUT="$2"; shift 2 ;;
    --duration)   DUR="$2"; shift 2 ;;
    --keys)       KEYS="$2"; shift 2 ;;
    --capture-at) CAPTURE_AT="$2"; shift 2 ;;
    --display)    DISP="$2"; shift 2 ;;
    --binary)     BIN="$2"; shift 2 ;;
    --screen)     SCREEN="$2"; shift 2 ;;
    --clock)      CLOCK="$2"; shift 2 ;;
    --wait-for)   WAITFOR="$2"; shift 2 ;;
    --then-keys)  THENKEYS="$2"; shift 2 ;;
    --step-file)  STEPFILE="$2"; shift 2 ;;
    --wait-pulse) WAITPULSE="$2"; shift 2 ;;
    --wait-stall-timeout) WAIT_STALL_TIMEOUT="$2"; shift 2 ;;
    --startup-present-min) STARTUP_PRESENT_MIN="$2"; shift 2 ;;
    --startup-timeout) STARTUP_TIMEOUT="$2"; shift 2 ;;
    --)           shift; break ;;
    *)            echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

[ -n "$OUT" ] && [ -n "$DUR" ] || { echo "usage: $0 --out DIR --duration SEC" >&2; exit 2; }
[ -x "$BIN" ] || { echo "FATAL: no runnable binary at $BIN" >&2; exit 1; }
if [ -n "$STEPFILE" ]; then
  STEPFILE="$(readlink -f "$STEPFILE")"
  [ -f "$STEPFILE" ] || { echo "FATAL: no step file at $STEPFILE" >&2; exit 1; }
fi

for tool in Xvfb xdotool import flock; do
  command -v "$tool" >/dev/null || { echo "FATAL: $tool not on PATH" >&2; exit 1; }
done

BIN="$(readlink -f "$BIN")"
bindir="$(dirname "$BIN")"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
RUNTIME_LOG="$OUT/ac6recomp.log"

# The runtime used to force every invocation to write build-rt/ac6recomp.log.
# Two otherwise isolated displays could therefore overwrite each other's trace
# and contend during startup while both claimed valid evidence. Serialize runs
# of one binary; separate binaries remain independent.
exec 9>"$bindir/.ac6-run.lock"
if ! flock -n 9; then
  echo "FATAL: another AC6 run owns binary $BIN" >&2
  exit 1
fi

rm -f "$OUT"/*.png "$RUNTIME_LOG"

game_pid=""
xvfb_pid=""
cleanup() {
  if [ -n "$game_pid" ] && kill -0 "$game_pid" 2>/dev/null; then
    kill "$game_pid" 2>/dev/null || true
    wait "$game_pid" 2>/dev/null || true
  fi
  if [ -n "$xvfb_pid" ] && kill -0 "$xvfb_pid" 2>/dev/null; then
    kill "$xvfb_pid" 2>/dev/null || true
    wait "$xvfb_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

# Default capture schedule: every 15s, stopping short of the shutdown window.
if [ -z "$CAPTURE_AT" ]; then
  t=15
  while [ "$t" -lt "$DUR" ]; do CAPTURE_AT="${CAPTURE_AT:+$CAPTURE_AT,}$t"; t=$((t + 15)); done
fi

if pgrep -f "[X]vfb ${DISP}( |$)" >/dev/null; then
  echo "FATAL: display $DISP is already owned by another Xvfb" >&2
  exit 1
fi

nohup Xvfb "$DISP" -screen 0 "${SCREEN}x24" >/dev/null 2>&1 &
xvfb_pid=$!
sleep 3
kill -0 "$xvfb_pid" 2>/dev/null || { echo "FATAL: Xvfb did not start on $DISP" >&2; exit 1; }

# --mnk_mode=true is required: the keyboard/mouse driver is OFF by default and
# reports DEVICE_NOT_CONNECTED, so the guest sees no pad at all and no key
# reaches it whatever the guest does with it.
#
# Run from the binary's own directory so relative game assets keep resolving,
# but put the runtime log directly in the evidence directory.
cd "$bindir" || exit 1
DISPLAY="$DISP" nohup timeout -k 5 "$DUR" "$BIN" \
    --mnk_mode=true --ac6_performance_mode=false --log_flush_interval=1 \
    "$@" --log_file="$RUNTIME_LOG" > "$OUT/run.log" 2>&1 &
game_pid=$!
game_started_epoch=$(date +%s)

if [ "$STARTUP_PRESENT_MIN" -gt 0 ]; then
  startup_waited=0
  startup_presents=0
  while [ "$startup_presents" -lt "$STARTUP_PRESENT_MIN" ]; do
    if ! kill -0 "$game_pid" 2>/dev/null; then
      wait "$game_pid" 2>/dev/null || startup_rc=$?
      game_pid=""
      echo "FATAL: AC6 exited during startup (status ${startup_rc:-0})" >&2
      exit "${startup_rc:-1}"
    fi
    if [ "$startup_waited" -ge "$STARTUP_TIMEOUT" ]; then
      echo "FATAL: startup published fewer than $STARTUP_PRESENT_MIN frames in ${STARTUP_TIMEOUT}s" >&2
      exit 1
    fi
    sleep 1
    startup_waited=$((startup_waited + 1))
    if [ -s "$RUNTIME_LOG" ]; then
      startup_presents="$(grep -c 'PRESENT' "$RUNTIME_LOG" 2>/dev/null || :)"
      startup_presents="${startup_presents:-0}"
    fi
  done
  echo "startup healthy: ${STARTUP_PRESENT_MIN} presentations in ${startup_waited}s"
fi

# Focus is taken immediately before every key, not once at startup.
#
# Focusing at t=5s and trusting it for the rest of the run silently loses every
# press: the window that exists during early boot is not necessarily the one
# receiving input later, and an unfocused MnK driver drops keys without
# reporting anything. A whole run then reads as "the guest ignored the input"
# when the input never arrived -- which is the single most expensive way to be
# wrong here, because it is indistinguishable from the defect under
# investigation.
focus_game() {
  local win
  # SDL publishes WM_CLASS as instance "ac6recomp", class "Ac6recomp".
  # xdotool's --class match is case-sensitive on this Xvfb stack, so the old
  # lower-case class query intermittently found no focusable window and silently
  # dropped an otherwise correctly timed press. Match the stable instance.
  win="$(DISPLAY=$DISP xdotool search --classname ac6recomp 2>/dev/null | tail -1)"
  if [ -n "$win" ]; then
    DISPLAY=$DISP xdotool windowactivate "$win" 2>/dev/null
    DISPLAY=$DISP xdotool windowfocus "$win" 2>/dev/null
    return 0
  fi
  return 1
}

# Merge the key events and the capture times into one ordered timeline, so a
# capture scheduled just after a press actually lands after it.
timeline="$(
  { IFS=','; for k in $KEYS;       do [ -n "$k" ] && echo "${k%%:*} key $k"; done
    IFS=','; for c in $CAPTURE_AT; do [ -n "$c" ] && echo "$c cap $c"; done
  } | sort -n -k1,1
)"

# Block until the runtime log says the guest has reached a given state.
#
# This is the reliable form of "pace the input on the executable, not the host".
# --clock present counts PRESENT lines, but those are emitted by the swap path
# and are not one per frame on every screen: a capture taken mid-run showed the
# overlay reporting a healthy 60.56 fps while the PRESENT count had almost
# stopped, so a frame-count schedule stalls for reasons that have nothing to do
# with the game's progress. Waiting for a state the guest itself announces has
# no such failure mode.
#
# --wait-for "\[ac6-screen-id\]" holds the timeline until the dialog screen
# actually exists, which is what every experiment on that dialog needs.
wait_for_log() {
  local pattern="$1" limit="$2" waited=0
  local pulse_key_sequence="" pulse_hold="" pulse_interval=2
  local last_present_count=-1 stagnant=0 present_count=0
  if [ -n "$WAITPULSE" ]; then
    pulse_key_sequence="$(echo "$WAITPULSE" | cut -d: -f1)"
    pulse_hold="$(echo "$WAITPULSE" | cut -d: -f2)"
    pulse_interval="$(echo "$WAITPULSE" | cut -d: -f3)"
    pulse_hold="${pulse_hold:-0.1}"
    pulse_interval="${pulse_interval:-2}"
  fi
  echo "  waiting for /$pattern/ in the runtime log (up to ${limit}s)"
  # The runtime may rotate the configured log file while an instrumented
  # route is running.  Search the complete evidence directory, not only the
  # current file; otherwise a state emitted just before rotation is reported
  # as missing and the harness can abort a healthy game.
  while [ "$(log_match_count "$pattern")" -eq 0 ]; do
    if [ -n "$pulse_key_sequence" ] && focus_game; then
      IFS='+' read -ra pulse_keys <<< "$pulse_key_sequence"
      for pulse_key in "${pulse_keys[@]}"; do
        DISPLAY=$DISP xdotool keydown "$pulse_key" 2>/dev/null
        sleep "$pulse_hold"
        DISPLAY=$DISP xdotool keyup "$pulse_key" 2>/dev/null
      done
      unset IFS
    fi
    sleep "$pulse_interval"
    waited=$((waited + pulse_interval))
    kill -0 "$game_pid" 2>/dev/null || { echo "  (game exited while waiting)"; return 1; }
    if [ "$WAIT_STALL_TIMEOUT" -gt 0 ]; then
      present_count="$(guest_frames)"
      if [ "$present_count" -eq "$last_present_count" ]; then
        stagnant=$((stagnant + pulse_interval))
      else
        stagnant=0
        last_present_count="$present_count"
      fi
      if [ "$stagnant" -ge "$WAIT_STALL_TIMEOUT" ]; then
        echo "  (presentation stalled at $present_count for ${stagnant}s)"
        return 1
      fi
    fi
    [ "$waited" -ge "$limit" ] && { echo "  (gave up waiting for /$pattern/)"; return 1; }
  done
  echo "  matched /$pattern/ after ${waited}s"
  return 0
}

# Advance the timeline on the guest's clock, not the host's.
#
# A wall-clock schedule assumes the game reaches a given screen at a given
# second. It does not: an instrumented build runs at 9-32 FPS against 60, and
# even two runs of the same binary vary, so a press aimed at the dialog lands in
# the cutscene and the run reports that the guest ignored it. Three captures
# were lost to this before the cause was clear.
#
# --clock present counts the runtime's own per-frame PRESENT lines instead, so
# positions in --keys and --capture-at are guest frames. The same schedule then
# lands on the same screen whatever the host speed.
guest_frames() {
  local count=0 file matches
  for file in "$RUNTIME_LOG" "$OUT"/ac6recomp.*.log; do
    [ -f "$file" ] || continue
    matches="$(grep -c "PRESENT" "$file" 2>/dev/null || :)"
    count=$((count + ${matches:-0}))
  done
  printf '%s\n' "$count"
}

# The runtime rotates a fixed log_file when an instrumented run is noisy. A
# state-driven recipe must observe all rotations, otherwise a state written to
# ac6recomp.1.log is indistinguishable from a guest hang. The output directory
# is fresh per run (log_new_file_per_launch=true), so counting a pattern across
# the current file and its siblings is sufficient to preserve the
# "after the previous match" invariant.
log_match_count() {
  local pattern="$1" count=0 file matches
  for file in "$RUNTIME_LOG" "$OUT"/ac6recomp.*.log; do
    [ -f "$file" ] || continue
    if matches="$(grep -E -c -- "$pattern" "$file" 2>/dev/null)"; then
      :
    else
      matches=0
    fi
    count=$((count + matches))
  done
  printf '%s\n' "$count"
}

wait_until() {
  local target="$1" waited=0
  if [ "$CLOCK" = "present" ]; then
    while [ "$(guest_frames)" -lt "$target" ]; do
      sleep 1
      waited=$((waited + 1))
      # Never outlive the game: if it has exited, the frame count is frozen and
      # this would spin until the script's own timeout.
      kill -0 "$game_pid" 2>/dev/null || { echo "  (game gone, abandoning timeline)"; return 1; }
      [ "$waited" -gt "$DUR" ] && { echo "  (timeline gave up waiting for frame $target)"; return 1; }
    done
  else
    local delta
    delta=$(awk -v a="$target" -v n="$now" 'BEGIN{d=a-n; print (d>0)?d:0}')
    sleep "$delta"
  fi
  now="$target"
  return 0
}

clock_unit="s"; [ "$CLOCK" = "present" ] && clock_unit="f"

now=${startup_waited:-0}
while read -r at kind payload; do
  [ -n "$at" ] || continue
  wait_until "$at" || break
  case "$kind" in
    key)
      key="$(echo "$payload" | cut -d: -f2)"
      hold="$(echo "$payload" | cut -d: -f3)"; hold="${hold:-0.5}"
      if focus_game; then
        DISPLAY=$DISP xdotool keydown "$key" 2>/dev/null
        sleep "$hold"
        DISPLAY=$DISP xdotool keyup "$key" 2>/dev/null
        echo "t=${at}${clock_unit}  held $key for ${hold}s"
      else
        echo "t=${at}${clock_unit}  SKIPPED $key -- no ac6recomp window to focus" >&2
      fi
      ;;
    cap)
      if DISPLAY=$DISP timeout 20 import -window root "$OUT/t${at}.png" 2>/dev/null; then
        alive=0; kill -0 "$game_pid" 2>/dev/null && alive=1
        echo "t=${at}${clock_unit}  captured  (owned game alive: $alive)"
      else
        echo "t=${at}${clock_unit}  capture FAILED"
      fi
      ;;
  esac
done <<< "$timeline"

# The pre-sequence above only has to get the guest moving -- leave the title,
# skip the cutscene -- and its exact timing does not matter. The press the
# experiment is actually about does matter, and it must land on the screen under
# test, so it waits for the guest to announce that screen rather than for a
# clock. Every capture lost so far was a press that arrived before the dialog
# existed and was recorded as the guest ignoring it.
if [ -n "$WAITFOR" ]; then
  if wait_for_log "$WAITFOR" "$DUR"; then
    post_index=0
    IFS=','; for k in $THENKEYS; do
      [ -n "$k" ] || continue
      post_index=$((post_index + 1))
      key="$(echo "$k" | cut -d: -f2)"
      hold="$(echo "$k" | cut -d: -f3)"; hold="${hold:-0.6}"
      sleep "$(echo "$k" | cut -d: -f1)"
      if [ "$key" = "CAPTURE" ]; then
        echo "  (post-wait) observation only"
      elif focus_game; then
        DISPLAY=$DISP xdotool keydown "$key" 2>/dev/null
        sleep "$hold"
        DISPLAY=$DISP xdotool keyup "$key" 2>/dev/null
        echo "  (post-wait) held $key for ${hold}s"
      else
        echo "  (post-wait) SKIPPED $key -- no window to focus" >&2
      fi
      post_capture="$(printf '%s/after-%02d-%s.png' "$OUT" "$post_index" "$key")"
      DISPLAY=$DISP timeout 20 import -window root "$post_capture" 2>/dev/null \
        && echo "  (post-wait) captured $(basename "$post_capture")"
    done
    unset IFS
  else
    echo "FATAL: initial state /$WAITFOR/ was not reached" >&2
    exit 67
  fi
fi

# State-driven recipes are used for long routes where wall-clock input is not
# evidence. Each `wait` sees only log lines written after the previous match,
# so a stale dialog type from earlier in the run cannot satisfy a later stage.
if [ -n "$STEPFILE" ]; then
  step_index=0
  step_log_line="$(wc -l < "$RUNTIME_LOG" 2>/dev/null || echo 0)"
  step_failed=0
  declare -A step_seen_counts=()
  while IFS=$'\t' read -r operation argument limit remainder || [ -n "$operation" ]; do
    case "$operation" in
      ""|\#*) continue ;;
    esac
    step_index=$((step_index + 1))
    case "$operation" in
      wait)
        [ -n "$argument" ] && [ -n "$limit" ] || {
          echo "FATAL: malformed wait at step $step_index" >&2
          step_failed=1
          break
        }
        step_waited=0
        # Snapshot only after a successful match of this same pattern. Taking
        # the snapshot here would race a fast guest: the key immediately before
        # this wait can already have produced the expected line.
        step_match_before="${step_seen_counts[$argument]:-0}"
        echo "  (step $step_index) waiting for /$argument/ after ${step_match_before} prior matches"
        while [ "$(log_match_count "$argument")" -le "$step_match_before" ]; do
          kill -0 "$game_pid" 2>/dev/null || {
            echo "FATAL: game exited during step $step_index" >&2
            step_failed=1
            break
          }
          [ "$step_waited" -lt "$limit" ] || {
            echo "FATAL: step $step_index timed out waiting for /$argument/" >&2
            step_failed=1
            break
          }
          sleep 1
          step_waited=$((step_waited + 1))
        done
        [ "$step_failed" -eq 0 ] || break
        step_seen_counts[$argument]="$(log_match_count "$argument")"
        step_log_line="$(wc -l < "$RUNTIME_LOG" 2>/dev/null || echo 0)"
        echo "  (step $step_index) matched after ${step_waited}s"
        ;;
      wait-pulse)
        [ -n "$argument" ] && [ -n "$limit" ] || {
          echo "FATAL: malformed wait-pulse at step $step_index" >&2
          step_failed=1
          break
        }
        pulse_waited=0
        pulse_match_before="${step_seen_counts[$argument]:-0}"
        echo "  (step $step_index) pulsing $limit while waiting for /$argument/"
        while [ "$(log_match_count "$argument")" -le "$pulse_match_before" ]; do
          kill -0 "$game_pid" 2>/dev/null || {
            echo "FATAL: game exited during wait-pulse step $step_index" >&2
            step_failed=1
            break
          }
          if focus_game; then
            DISPLAY=$DISP xdotool keydown "$limit" 2>/dev/null
            sleep 0.1
            DISPLAY=$DISP xdotool keyup "$limit" 2>/dev/null
          else
            echo "FATAL: no focusable game window during wait-pulse step $step_index" >&2
            step_failed=1
            break
          fi
          sleep 2
          pulse_waited=$((pulse_waited + 2))
          [ "$pulse_waited" -lt "$DUR" ] || {
            echo "FATAL: wait-pulse step $step_index timed out waiting for /$argument/" >&2
            step_failed=1
            break
          }
        done
        [ "$step_failed" -eq 0 ] || break
        step_seen_counts[$argument]="$(log_match_count "$argument")"
        echo "  (step $step_index) pulse matched after ${pulse_waited}s"
        ;;
      key)
        [ -n "$argument" ] || {
          echo "FATAL: missing keysym at step $step_index" >&2
          step_failed=1
          break
        }
        hold="${limit:-0.1}"
        if focus_game; then
          DISPLAY=$DISP xdotool keydown "$argument" 2>/dev/null
          sleep "$hold"
          DISPLAY=$DISP xdotool keyup "$argument" 2>/dev/null
          echo "  (step $step_index) held $argument for ${hold}s"
        else
          echo "FATAL: no focusable game window at step $step_index" >&2
          step_failed=1
          break
        fi
        ;;
      present)
        [ -n "$argument" ] && [ -n "$limit" ] || {
          echo "FATAL: malformed present gate at step $step_index" >&2
          step_failed=1
          break
        }
        present_start="$(guest_frames)"
        present_target=$((present_start + argument))
        present_waited=0
        while [ "$(guest_frames)" -lt "$present_target" ]; do
          kill -0 "$game_pid" 2>/dev/null || {
            echo "FATAL: game exited during present gate $step_index" >&2
            step_failed=1
            break
          }
          [ "$present_waited" -lt "$limit" ] || {
            echo "FATAL: present gate $step_index stalled at $(guest_frames)/$present_target" >&2
            step_failed=1
            break
          }
          sleep 1
          present_waited=$((present_waited + 1))
        done
        [ "$step_failed" -eq 0 ] || break
        echo "  (step $step_index) presentation advanced by $argument"
        ;;
      sleep)
        [ -n "$argument" ] || {
          echo "FATAL: missing sleep duration at step $step_index" >&2
          step_failed=1
          break
        }
        sleep "$argument"
        ;;
      capture)
        case "$argument" in
          ""|*[!A-Za-z0-9._-]*)
            echo "FATAL: unsafe capture label at step $step_index" >&2
            step_failed=1
            break
            ;;
        esac
        step_capture="$(printf '%s/step-%02d-%s.png' "$OUT" "$step_index" "$argument")"
        if DISPLAY=$DISP timeout 20 import -window root "$step_capture" 2>/dev/null; then
          echo "  (step $step_index) captured $(basename "$step_capture")"
        else
          echo "FATAL: capture failed at step $step_index" >&2
          step_failed=1
          break
        fi
        ;;
      *)
        echo "FATAL: unknown step operation '$operation' at step $step_index" >&2
        step_failed=1
        break
        ;;
    esac
  done < "$STEPFILE"
  [ "$step_failed" -eq 0 ] || exit 68
fi

game_rc=0
wait "$game_pid" 2>/dev/null || game_rc=$?
game_elapsed=$(( $(date +%s) - game_started_epoch ))
game_pid=""
cleanup
xvfb_pid=""

# The runtime writes directly into the evidence directory. Reject a run that
# somehow produced no trace rather than copying a shared/stale build-tree log.
if [ ! -s "$RUNTIME_LOG" ]; then
  echo "WARNING: no runtime log at $RUNTIME_LOG" >&2
fi

echo "--- captures in $OUT ---"
ls -1 "$OUT"/*.png 2>/dev/null || echo "(none)"

# timeout(1) returns 124 when the requested bounded run completes, or 137 when
# its -k grace period expires and it has to SIGKILL a runtime that ignores TERM.
# Accept 137 only after the requested duration. An early SIGBUS/SIGKILL remains
# a failed experiment: the old harness swallowed the former and let run-once
# record a crashing memory scanner as PASS.
bounded_kill=0
if [ "$game_rc" -eq 137 ] && awk -v elapsed="$game_elapsed" -v duration="$DUR" \
    'BEGIN { exit !((elapsed + 1) >= duration) }'; then
  bounded_kill=1
  echo "bounded run completed; timeout killed the runtime after ${game_elapsed}s"
fi
if [ "$game_rc" -ne 0 ] && [ "$game_rc" -ne 124 ] && [ "$bounded_kill" -ne 1 ]; then
  echo "FATAL: owned ac6recomp exited with status $game_rc" >&2
  exit "$game_rc"
fi
