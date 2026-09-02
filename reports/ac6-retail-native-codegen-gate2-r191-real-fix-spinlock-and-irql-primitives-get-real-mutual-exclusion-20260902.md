# AC6 retail NTSC-U/J — real fix: spinlock/IRQL kernel primitives get real mutual exclusion (r191)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(178/178, up from 177/177).

## What was found

`KfAcquireSpinLock`, `KfReleaseSpinLock`, `KeAcquireSpinLockAtRaisedIrql`,
`KeReleaseSpinLockFromRaisedIrql`, `KeRaiseIrqlToDpcLevel`, `KfLowerIrql`
were all still the generic offline no-op (`kOfflineStatus`). These are
standard, documented Xbox 360/NT kernel signatures (protocol-level external
knowledge, same class this project already accepts for `XINPUT_STATE` and
`TIME_FIELDS` — distinct from a compiler-specific struct offset that must be
read from this XEX):

- `KIRQL KfAcquireSpinLock(PKSPIN_LOCK SpinLock)` / `VOID
  KfReleaseSpinLock(PKSPIN_LOCK SpinLock, KIRQL NewIrql)`;
- `VOID KeAcquireSpinLockAtRaisedIrql(PKSPIN_LOCK SpinLock)` / `VOID
  KeReleaseSpinLockFromRaisedIrql(PKSPIN_LOCK SpinLock)` — same lock object
  identity, caller already at raised IRQL;
- `KIRQL KeRaiseIrqlToDpcLevel(VOID)` / `VOID KfLowerIrql(KIRQL NewIrql)` —
  no lock object parameter.

This XEX's own disassembly at `0x821e5fd0` confirms real use: `bl
0x823d045c` (`KfAcquireSpinLock`) at `0x821e600c` and `bl 0x823d047c`
(`KfReleaseSpinLock`) at `0x821e6060` bracket a real queue-append
(`0x821e6020`-`0x821e6054`: OR'ing flags, storing a list-node payload,
advancing a count field at `+0x2af8`). `tools/count_indirect_branches.py`
was not needed here — both call targets are direct `bl`s, not dispatched
through a table. Real call-site counts across this XEX run from the
dozens (the two `KeAcquireSpinLockAtRaisedIrql`/`KeReleaseSpinLockFromRaisedIrql`
pair) to 88-110 (`KeRaiseIrqlToDpcLevel`/`KfLowerIrql`) — this primitive
family spans most of the engine's address range, not one isolated site.

This project's own prior work (r111-r115) confirmed real concurrent host
threads via `ExCreateThread`, and r116 fixed the same class of gap for
`RtlEnterCriticalSection`/`RtlLeaveCriticalSection`. The spinlock/IRQL
family is the same real-concurrency risk for a more pervasively-used
primitive, previously untouched.

## Fix

- `spin_lock_for(key)`: a new helper, directly modeled on r116's
  `critical_section_for(key)`, keyed by the guest `KSPIN_LOCK` object's own
  address. Plain (non-recursive) `std::mutex`, not `std::recursive_mutex`:
  a real spinlock is not re-entrant either — self-reacquisition deadlocks
  on real hardware too, so a non-recursive mutex matches real semantics
  rather than papering over a guest bug.
- `KfAcquireSpinLock`/`KeAcquireSpinLockAtRaisedIrql` lock it;
  `KfReleaseSpinLock`/`KeReleaseSpinLockFromRaisedIrql` unlock it. The
  "old IRQL" `KfAcquireSpinLock`/`KeRaiseIrqlToDpcLevel` return is set to
  `PASSIVE_LEVEL` (0): this cycle did not trace any of this XEX's own call
  sites reading that return value, so no other value is asserted — the
  only value real code running below `DISPATCH_LEVEL` can ever observe
  itself at is `PASSIVE_LEVEL`.
- `g_dpc_level_mutex`: one global lock for `KeRaiseIrqlToDpcLevel`/
  `KfLowerIrql`, since these two are IRQL-only and unpaired with a
  specific lock object. Unlike `spin_lock_for`, this **is**
  `std::recursive_mutex`. Real IRQL is per-thread state, not object
  identity: the same thread legitimately raises to DPC level while
  already there (a nested Raise/Lower pair from one thread is routine
  kernel control flow, not a self-reacquisition of the same object a real
  spinlock forbids). A plain mutex here would self-deadlock on the first
  nested Raise from the thread already holding it — a real correctness
  regression this fix does not introduce.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/` source
file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
178/178 (177/177 before this cycle, +1 new test,
`test_spinlock_and_irql_primitives_use_real_mutual_exclusion`). `git status`
unchanged apart from the intended two-file change set and the same
pre-existing, unrelated dirty state noted by every prior cycle in this
series (the `reconstruction/ace-combat-6` mission01-campaign-loader/
free-flight WIP and the archived `recompilation/ace-combat-6-demo` tree).

`tools/audit_ac6_mission01_native_gate.py` currently fails
(`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp`) — pre-existing,
unrelated to this change: that file sits in the same out-of-scope
`reconstruction/ace-combat-6` WIP named above, not touched by this cycle,
and was already dirty before this cycle started. Not staged or fixed here;
belongs to a different investigative thread.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r190).
2. If a future call site is found reading the "old IRQL" return value from
   `KfAcquireSpinLock`/`KeRaiseIrqlToDpcLevel`, trace it before asserting
   the current `PASSIVE_LEVEL` constant is correct in that context.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
