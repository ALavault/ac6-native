# AC6 retail NTSC-U/J — real fix: `KeLockL2`/`KeUnlockL2`/`KiApcNormalRoutineNop` always succeed (r197)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(184/184, up from 183/183).

## What was found

Three still-no-op imports, each with a single real call site, share the
same shape: the caller discards the return value entirely (falls straight
into the next instruction, no `cmpwi`/branch on `r3` at all).

- `KeLockL2`/`KeUnlockL2` (real call sites `0x821eded0`, `0x821eea94`):
  real hardware L2-cache-way locking, a performance-tuning primitive with
  no host-side equivalent to emulate — there is no cache-way partitioning
  for a host interpreter to honor.
- `KiApcNormalRoutineNop` (real call site `0x821e6908`): a documented
  no-op by its own name and purpose — the default "NormalRoutine"
  callback for a kernel APC that has no real user-mode routine.

Since all three call sites ignore the return value, the generic offline
fallback's only actual defect here was diagnostic noise (an unnecessary
`trace_offline_import`/`kOfflineStatus` write for a call whose result
nothing reads), not any observable behavior difference — still worth
fixing to keep the offline-import trace signal limited to imports that
matter.

## Fix

All three now return `STATUS_SUCCESS`/`0` unconditionally, with no other
side effect — matching the "native side has nothing to do here" precedent
already used for `VdRetrainEDRAM` and similar lifecycle no-ops.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
184/184 (183/183 before this cycle, +1 new test,
`test_l2_lock_and_apc_nop_always_succeed`). `git status` unchanged apart
from the intended change set and the same pre-existing, unrelated dirty
state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r196's reports, not touched by
this cycle.

## Also checked, no fix: `XamSessionCreateHandle`/`XamSessionRefObjByHandle`, `NtDuplicateObject`

`XamSessionRefObjByHandle` has 11 real call sites (and its likely paired
`XamSessionCreateHandle` has 1), all inside a cluster of near-identical
wrapper functions around `0x821fd3e8`-`0x821fd9xx` (the same
neighborhood as r195's `XamAlloc`/`XamFree` fix). Traced one pair
directly (`0x821fd598`/`0x821fd5b4`, inside `Function_821FD4C0`): the
resolved "object" pointer is written into a struct passed to an internal
(non-imported) function at `0x823cfe4c` alongside constants that look
like a fixed telemetry/event-tracing record (event id `0xfb`, per-caller
tag values `0x10`-`0x13` across the sibling wrapper functions) — not
dereferenced as real data downstream in what this cycle traced. Fixing
this honestly would mean building a session-handle-to-object registry
backing at least 12 call sites without being certain what the "object"
needs to be beyond a non-null token; this cycle did not trace enough of
the family to be confident the fix wouldn't be cosmetic guesswork dressed
up as a real fix. Left as the generic offline no-op, named honestly
rather than forced.

`NtDuplicateObject` has 1 real call site (`0x82390be0`, inside
`Function_82390BC8`) that passes only 3 registers to the call
(`r3`/`r4`/`r5`) and captures no output handle at all — unusual for a
"duplicate object" contract, which normally needs a `PHANDLE` out
parameter. Whether this XEX's build uses a reduced-arity kernel thunk
under the same import name, or whether the 3-register pattern is this
particular wrapper discarding an unused duplicate, was not resolved this
cycle. Left as the generic offline no-op rather than guess the missing
argument's role.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r197).
2. If `XamSessionCreateHandle`/`XamSessionRefObjByHandle` are revisited,
   trace at least 2-3 more of the sibling wrapper functions
   (`0x821fd660`, `0x821fd718`, `0x821fd7c8`, `0x821fd870`, `0x821fd918`)
   before implementing, to confirm whether the "object" pointer is ever
   actually dereferenced anywhere, not just logged.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
