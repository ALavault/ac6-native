# AC6 retail NTSC-U/J — real fix: `ExTerminateThread` unwinds its own thread cleanly (r213)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to both a generated-stub source
(`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`)
**and** two real `native/` sources
(`native/include/ac6/native_runtime.h`, `native/src/ac6recomp_main.cpp`),
with matching tests. `tools/prepare.py --target ntsc-uj --profile native`
was re-run first (per this project's own `native/`-edit discipline) and
`diff -rq native/ build/ntsc-uj/native/native-source/` confirmed no
differences before building. Verified via a clean `tools/build.py
--target ntsc-uj --profile native` (`ctest` 10/10) and the full
retail-native pytest suite (199/199, up from 196/196).

## What was found

`VOID ExTerminateThread(DWORD ExitCode)` is a documented, never-returning
NT kernel API. This XEX has 2 real call sites (`0x821f8060`,
`0x82390b38`). The second is conclusive, the same evidence class r193
(`KeBugCheck`) and r209 (`XamLoaderTerminateTitle`) already established:
the instruction immediately after it (`0x82390b40`) is a **different
function's own prologue** — the compiler emitted no epilogue at all
after this call.

Unlike those two, `ExTerminateThread` ends only the **calling thread**,
not the whole process or title. This project's own `ExCreateThread`
(r111-r115) already spawns each guest thread as a real, detached
`std::thread` running `shim(worker, base)` as a plain C++ function call —
so a bare `throw` from deep inside the recompiled call graph would
correctly unwind all the way out of `shim()`, but then escape the
`std::thread` entry point itself, which C++ turns into an unconditional
`std::terminate()` call on the **entire process** for what should only
be one thread ending. Neither `std::abort()` (r193's precedent, wrong
here — this is not a fault) nor `std::exit()` (r209's precedent, wrong
here — this is not a title-wide exit) is the correct match.

## Fix

Added a small marker type, `ac6::native::GuestThreadTerminated`, to the
real `native/include/ac6/native_runtime.h` header (shared between the
generated stubs and `native/` proper). `ExTerminateThread` throws it.
Every point that directly invokes a guest thread's own C++ entry function
now catches it and lets that thread return normally afterward instead of
letting the exception escape:

- `ExCreateThread`'s spawned `std::thread` lambda (in the generated
  stub) — the primary, expected case, since `ExTerminateThread` on real
  hardware is normally a background/worker-thread's own exit path.
- The main entry-point probe call in `native/src/ac6recomp_main.cpp`
  (`entry_function(...)`) — the one other place this project directly
  invokes a guest thread's C++ entry point outside `ExCreateThread`.

`ExRegisterTitleTerminateNotification` (9 real call sites, 2 traced
directly: `0x821f1190`, `0x821f1620`) was fixed alongside it in the same
cycle: every real call site discards its return value outright, the same
class of fix as r197's `KeLockL2`/`KeUnlockL2` — this project has nowhere
to invoke an arbitrary registered cleanup callback from later, but since
nothing reads the return here, that gap is not observable.

## Gates

`ctest` 10/10 (native profile, target count unchanged). Full
retail-native pytest suite: 199/199 (196/196 before this cycle, +3 new
tests: `test_ex_terminate_thread_throws_instead_of_returning`,
`test_ex_create_thread_catches_guest_thread_terminated`,
`test_ex_register_title_terminate_notification_always_succeeds`). `git
status` — 239 tracked changes (235 pre-existing baseline + the 4 files
this cycle actually touched: the two files above and this cycle's two
`native/` edits), unrelated dirty state unchanged.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r212's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r213).
2. If a future cycle adds another direct invocation site for a guest
   thread's own C++ entry function (outside `ExCreateThread` and the
   main probe), it needs the same `GuestThreadTerminated` catch.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
