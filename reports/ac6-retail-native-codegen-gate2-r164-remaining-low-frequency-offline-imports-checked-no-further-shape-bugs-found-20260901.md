# AC6 retail NTSC-U/J — remaining low-frequency offline-import stubs checked; no further contract-shape bugs found (r164)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, `-readOnly -noanalysis`. XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle — a static-only check, no
diagnostic added or reverted, no build touched.

## Why this check was worth running

r163 closed the `ObDereferenceObject`/`KeSetBasePriorityThread`/
`KeQueryBasePriorityThread` family and named "a fresh scan of the live
import trace for any other still-unaddressed high-call-volume
offline-import stub" as the natural next check.

## What was found

A fresh `AC6_NATIVE_IMPORT_TRACE=1` bounded probe shows every remaining
offline-import stub hit at most twice in this run — no candidate at the
call-volume tier `ObDereferenceObject` (35 hits) or
`KeSetBasePriorityThread` (17 hits) were at before their own fixes.

The two most plausible remaining candidates for the same "NTSTATUS-shaped
sentinel where the real contract is a plain boolean/void" bug category
were checked against their real static call sites (`Ac6Xrefs.java` +
`Ac6XenonDisasm`), following the same discipline as r162/r163:

- **`RtlTryEnterCriticalSection`** (5 real static call sites, thunk
  `0x823D00AC`): its real contract is `BOOLEAN` (nonzero = lock
  acquired). `sub_8233CF78`'s call site does check the return value
  (`cmplwi r3,0x0 / beq <skip-critical-section>`), so this is not
  discarded — but the generic offline-import fallback's `kOfflineStatus`
  (`0xC00000BB`) is already nonzero, which already evaluates as "lock
  acquired" under this branch's own zero-vs-nonzero test. **The wrong
  contract shape does not currently change control flow at this site** —
  fixing it to return a canonical `1` instead of `kOfflineStatus` would be
  a pure cosmetic correctness improvement with no observed behavioral
  effect, unlike r163's genuine bug.
- **`KeEnterCriticalRegion`/`KeLeaveCriticalRegion`** (thunks `0x823D068C`/
  `0x823D067C`, 2 real static call sites each): real contract is `VOID`.
  `sub_821F3540`'s call to `KeEnterCriticalRegion` has its return value
  (`r3`) immediately overwritten by the next instruction
  (`lwz r3,0x5f4(r28)`) before anything reads it — genuinely discarded,
  matching the real `VOID` contract's own expectation that nothing checks
  a return value at all.

## Decision

No further high-value fix is identified at this call-frequency tier.
Unlike r163's `KeQueryBasePriorityThread` (a real, observable bug),
neither of these two candidates currently changes guest control flow —
fixing their contract shape purely for defensive-correctness reasons (the
same principle behind r162's two zero-impact fixes) is optional, low
priority, and not undertaken this cycle absent a specific reason to spend
the effort. This closes the "scan remaining offline-imports" sub-thread
r163 opened, with a documented negative result rather than a silent stop.

## Gates

No native code changed, no build touched. `ctest`'s last-known state
(9/9, r163) stands unaffected. `git status` unchanged (only pre-existing,
unrelated dirty state).

## Next

Both of Gate 2's named frontiers are unchanged: DATA.TBL chain fully
traced and closed (r144/r153/r161); `IM_LOAD_IMMEDIATE`→SPIR-V
policy-blocked pending Xenos fetch-signature qualification. With this
scan complete and no further actionable native-runtime lever identified,
this is a natural pause point for the loop absent new direction or a
decision to invest in the two remaining known items: the pinned Wine/Xenia
route for NTSC-U/J (not yet attempted, r156/r157), or the cosmetic
`RtlTryEnterCriticalSection`/`KeEnterCriticalRegion`/`KeLeaveCriticalRegion`
contract-shape cleanups named above.
