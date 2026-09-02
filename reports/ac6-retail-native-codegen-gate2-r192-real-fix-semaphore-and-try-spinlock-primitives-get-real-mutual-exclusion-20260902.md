# AC6 retail NTSC-U/J — real fix: `KeInitializeSemaphore`/`KeReleaseSemaphore`/`KeTryToAcquireSpinLockAtRaisedIrql` get real mutual exclusion (r192)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(179/179, up from 178/178).

## What was found

While sweeping the remainder of the r191 spinlock/IRQL family, three more
still-no-op imports turned out to be the direct object-address (`Ke*`)
counterparts of primitives this project already models via handles (`Nt*`)
or via `spin_lock_for` (r191):

- `KeTryToAcquireSpinLockAtRaisedIrql` (real call site `0x823a8bf4`): a
  non-blocking counterpart of r191's `KfAcquireSpinLock`, same
  `PKSPIN_LOCK` object identity. The real call masks the return through
  `rlwinm r11,r3,0x0,0x18,0x1f` (low 8 bits), confirming a `BOOLEAN`
  acquired/not-acquired result, not a status code.
- `KeInitializeSemaphore` (real call site `0x823add7c`): real code
  initializes a standard `KSEMAPHORE` dispatcher-object header
  (self-referential list at `Semaphore+0x8`) immediately before this call
  — a real object, not a placeholder. `KeReleaseSemaphore` (real call
  sites `0x823ad268`, `0x823ad8d4`, both confirming `r3=Semaphore`,
  `r4=Increment=1`) was still the generic no-op alongside it, so any
  `KeWaitForSingleObject`/`KeWaitForMultipleObjects` on one of these
  semaphores (already correctly wired since an earlier cycle) would
  deterministically time out — the semaphore's own release path never
  reached the wait.

Both are standard, documented NT kernel signatures (protocol-level
external knowledge, same class already accepted for `XINPUT_STATE`/
`TIME_FIELDS`): `BOOLEAN KeTryToAcquireSpinLockAtRaisedIrql(PKSPIN_LOCK
SpinLock)`; `VOID KeInitializeSemaphore(PKSEMAPHORE Semaphore, LONG Count,
LONG Limit)`; `LONG KeReleaseSemaphore(PKSEMAPHORE Semaphore, KPRIORITY
Increment, LONG Adjustment, BOOLEAN Wait)`.

## Fix

- `KeTryToAcquireSpinLockAtRaisedIrql` reuses r191's `spin_lock_for(key)`
  and returns `try_lock()`'s result as the `BOOLEAN`.
- `KeInitializeSemaphore` reuses r145's auto-reset event model
  (`create_event`), keyed by the `Semaphore` object's own guest address —
  the same "`Ke*` variants operate on the object directly, not a handle"
  pattern this project already uses for `KeSetEvent`/`KeResetEvent`.
  Initial signaled state is `Count > 0`, matching r145's
  `NtCreateSemaphore` precedent exactly.
- `KeReleaseSemaphore` calls `set_event(key)`. Its previous-count return
  value is reported as `0`, unmodeled — same precedent as r145's
  `NtReleaseSemaphore` (this project's event model has no notion of a
  semaphore's real count beyond signaled/not) — but returned directly in
  `r3` here, since the real API returns it as the function result, not
  through an out-pointer the way `NtReleaseSemaphore` does.

`KeAcquireSpinLockAtRaisedIrql`/`KeReleaseSpinLockFromRaisedIrql` (the
non-`Try` pair) and `KfAcquireSpinLock`/`KfReleaseSpinLock`/
`KeRaiseIrqlToDpcLevel`/`KfLowerIrql` were already fixed in r191; this
cycle closes the remaining three primitives in the same broader family
that r191's initial sweep had not yet reached.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/` source
file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
179/179 (178/178 before this cycle, +1 new test,
`test_semaphore_and_try_spinlock_use_real_mutual_exclusion`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191's report, not touched by this
cycle.

## Also checked, no fix: `NtQueryInformationFile`

This cycle also traced `NtQueryInformationFile`'s single real call site
(`0x823908a4`, inside `Function_82390880`, real function range
`[0x82390880, 0x82390923]`). The full contract is:
`NtQueryInformationFile(handle, IoStatusBlock, out, 8,
FilePositionInformation=14)` to read the file's current write position,
immediately followed by two `NtSetInformationFile` calls (real call sites
`0x823908cc`/`0x823908f4`, confirmed against `NtSetInformationFile`'s own
8 real call sites XEX-wide) setting `FileEndOfFileInformation=20` and
`FileAllocationInformation=19` to that same position — a save-file
finalize/truncate-to-written-length sequence. This project's guest media
is read-only (r189/r190) with no write-position tracking; a real fix here
requires write support this cycle does not add. Not fixed — named
honestly, same "no safe fix without a larger prerequisite" pattern as
r178's `XexCheckExecutablePrivilege`. `NtSetInformationFile` itself
remains open for the same reason.

Also surveyed but deferred as out of scope for a bounded cycle: `sprintf`
(7 real call sites)/`_vsnprintf` (2 real call sites) are real varargs
formatters over the PPC calling convention — a correct fix needs a small
guest-ABI printf engine, not a one-line contract fix, and was not
attempted here.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r192).
2. `NtQueryInformationFile`/`NtSetInformationFile`'s save-finalize
   sequence and `sprintf`/`_vsnprintf`'s varargs engine are both real,
   scoped follow-up candidates if/when write support or a guest printf
   implementation becomes worthwhile on their own terms.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
