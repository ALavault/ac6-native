# AC6 retail NTSC-U/J — real fix: `KeBugCheck`/`KeBugCheckEx` abort instead of silently continuing (r193)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(180/180, up from 179/179).

## What was found

`VOID KeBugCheck(ULONG BugCheckCode)` and `VOID KeBugCheckEx(ULONG
BugCheckCode, ULONG_PTR Parameter1..4)` are standard, documented NT kernel
APIs (protocol-level external knowledge, same class already accepted for
`XINPUT_STATE`/`TIME_FIELDS`) that **never return** — a real hardware
bugcheck halts the console. Both were still the generic offline no-op,
which returns normally with `kOfflineStatus` in `r3`.

Real call sites confirmed via `scripts/FindSymbolReferences.java`
(distinguishing the two exact symbols, since a naive substring search on
"KeBugCheck" also matches "KeBugCheckEx" and double-counts):
`KeBugCheck` (`823d054c`) has 7 real references (6 `UNCONDITIONAL_CALL` at
`0x8238329c`, `0x82383344`, `0x821ed328`, `0x821ed47c`, `0x82389a88`,
`0x82386cd0`, plus 1 `UNCONDITIONAL_JUMP` at `0x823831dc` — a real tail
call); `KeBugCheckEx` (`823d03ec`) has 4 real `UNCONDITIONAL_CALL`
references (`0x821fa75c`, `0x821f9e74`, `0x821faa4c`, `0x821f90f4`).

The generic no-op's silent return is a genuine execute-past-fatal risk,
not a cosmetic status-shape bug: this XEX's own compiled code after a
`KeBugCheck`/`KeBugCheckEx` call site was never written with the
expectation of resuming there (the real API never returns), so letting
the recompiled guest fall through into it runs code in a state the
original console could never actually reach.

## Fix

Both print a diagnostic (`[KeBugCheck]`/`[KeBugCheckEx]` with the real
`BugCheckCode`/parameter values read straight from `r3`-`r7`, nothing
invented) to `stderr`, then call `std::abort()`. This matches the real
"never returns" contract instead of fabricating a continuation this XEX's
own compiled code does not expect.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
180/180 (179/179 before this cycle, +1 new test,
`test_kebugcheck_family_aborts_instead_of_returning`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191/r192's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r193).
2. If a `KeBugCheck`/`KeBugCheckEx` abort is ever actually observed at
   runtime, the printed code/parameters are real values straight from the
   guest call, sufficient to identify which real-hardware fault this
   build is reproducing rather than papering over.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
