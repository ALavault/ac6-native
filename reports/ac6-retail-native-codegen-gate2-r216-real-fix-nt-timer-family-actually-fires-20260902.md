# AC6 retail NTSC-U/J — real fix: `NtSetTimerEx`/`NtCancelTimer`/`NtCreateTimer` actually fire (r216)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(202/202, up from 201/201).

## What was found

`NtSetTimerEx`'s single real call site routes through a wrapper
(`0x82204c54`/`0x82390ab0`, the same wrapper r191/r192's exploration
already partially traced) that this cycle fully resolved. The real
8-argument NT signature (`HANDLE TimerHandle, PLARGE_INTEGER DueTime,
PTIMERAPCROUTINE TimerApcRoutine, TIMER_TYPE TimerType, PVOID
TimerContext, LONG Period, BOOLEAN ResumeContext, ULONG Flags`) fits the
PPC ABI's `r3`-`r10` directly, and the wrapper's own register shuffle
(`or r9,r5,r5; or r5,r6,r6; li r6,0x1; rlwinm r8,r8,0,0x18,0x1f`) maps
the wrapper's own simplified parameter order onto it. Concretely, at
this XEX's one real call site: `TimerApcRoutine=NULL` (no APC to
invoke), `TimerType=1` (`SynchronizationTimer`, forced literal by the
wrapper), `DueTime≈-101984` (100ns units, i.e. a relative ~10.2ms delay,
read directly from the stack slot the wrapper builds via `lis
r11,-0x2; ori r11,r11,0x7960`), `Period=0` (one-shot). The caller waits
on this timer through `NtWaitForSingleObjectEx` on the same handle — a
path this project's `g_events`/`wait_event()` machinery already
implements — not through the (unused, NULL) APC callback.

`kOfflineStatus` made `NtSetTimerEx` always fail, and — critically —
`NtCreateTimer` previously shared the generic handle-only stub with
`NtCreateMutant`, never registering the handle in `g_events` at all. So
even a real, correctly-firing timer would have made any
`NtWaitForSingleObjectEx` on it a guaranteed `STATUS_TIMEOUT`, the same
class of gap r145 already fixed for `NtCreateSemaphore`.

## Fix

- `NtCreateTimer` now registers its handle in `g_events`
  (auto-reset/non-manual, matching the real `SynchronizationTimer` type
  this XEX's own call site forces), split out of the shared stub with
  `NtCreateMutant` (which keeps the prior generic handle-only
  allocation — nothing in this project's own tracing shows it needs to
  participate in the wait/signal model).
- `NtSetTimerEx` reads the real 64-bit `DueTime` and spawns a detached
  `std::thread` (`set_timer()`) that sleeps for the relative delay (only
  the negative/relative form this XEX's own call site produces is
  handled, matching r194's `KeDelayExecutionThread` precedent), then
  calls `set_event()` on the timer's handle — repeating on the `Period`
  interval if nonzero, honoring the documented contract even though the
  one real call site observed uses `Period=0`.
- `NtCancelTimer` sets a per-timer cancellation flag (`cancel_timer()`)
  checked by the sleeping thread before each fire, so a cancelled timer
  never spuriously signals after the fact. The single real call site
  passes `CurrentState*=NULL`, so no previous-state output is written.

One pre-existing test
(`test_generic_fallback_is_traceable_and_still_returns_offline_status`)
used `NtCancelTimer` as its example of the generic-fallback shape;
retargeted to `NtDuplicateObject` (still generic, r197's deferred
ambiguous-signature case) rather than deleted.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
202/202 (201/201 before this cycle, +1 new test,
`test_nt_set_timer_ex_actually_fires_and_signals`; one pre-existing test
retargeted and one updated for the `NtCreateTimer`/`NtCreateMutant`
split, not added or removed). `git status` unchanged apart from the
intended two-file change set and the same pre-existing, unrelated dirty
state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r215's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r216).
2. If a future call site is found passing a positive (absolute) `DueTime`
   or a non-NULL `TimerApcRoutine` to `NtSetTimerEx`, implement that form
   only once traced — this cycle deliberately handled only the relative,
   no-APC form this XEX's own real call site produces.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
