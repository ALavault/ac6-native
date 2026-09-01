# AC6 retail NTSC-U/J — real native-runtime fix: `ObReferenceObjectByHandle` never wrote its output object pointer, stranding a `KeResumeThread` call on uninitialized stack memory (r148)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **Native code changed for real this cycle, kept, not
reverted**: `tools/materialize_native_import_stubs.py`
(`ObReferenceObjectByHandle`) and
`tests/test_materialize_native_import_stubs.py`.

## Following r147's own next step

r147 named checking for other observable effects of r145's semaphore fix
before investing further. The full import trace against the r145/r147
binary shows `ObReferenceObjectByHandle` as this run's single
highest-count unhandled import (35 calls, more than any other) --
falling through to the generic offline-import fallback (returns
`kOfflineStatus`, writes nothing) on every one of them.

## What was found: consistent, safe usage across every sampled call site

Reading four independent call sites
(`sub_821EF2xx`, `sub_821F3Dxx`, `sub_821F3Exx`, `sub_823ADCxx`)
confirms a single, consistent real signature --
`ObReferenceObjectByHandle(Handle, ObjectType, PVOID* Object)` -- and a
single usage pattern: every site immediately re-passes the returned
"object" to another kernel thread API
(`KeSetBasePriorityThread`/`KeQueryBasePriorityThread`/
`ObDereferenceObject`/`KeResumeThread`) as an opaque token. None of the
sampled sites dereference fields of the returned object directly. This
makes this project's own handles -- already-opaque integers -- a safe,
direct stand-in for the real object pointer.

## A real, separate bug this fix closes

One call site (inside the code generated from `0x823ADCD4`-area,
following an `ExCreateThread` call) does not even check
`ObReferenceObjectByHandle`'s own return status before immediately
calling `KeResumeThread` on its output. Since the prior offline fallback
never wrote anything to `*Object`, `KeResumeThread` was called with
whatever uninitialized stack garbage happened to occupy that slot --
**not** the real thread handle `ExCreateThread` had just produced.
`KeResumeThread` (already correctly wired to this project's
`set_event()`/thread-park mechanism since r114) never received the
handle it needed to actually resume the worker thread that `ExCreateThread`
parked under `CREATE_SUSPENDED`. This is a genuine, previously-unnoticed
thread-resume bug, not merely a missing-import status code.

## The fix

`ObReferenceObjectByHandle` now writes the real handle straight through
as the "object" (`if (ctx.r5.u32 != 0u) PPC_STORE_U32(ctx.r5.u32,
ctx.r3.u32);`) and returns `STATUS_SUCCESS`. `KeSetBasePriorityThread`,
`KeQueryBasePriorityThread`, `ObDereferenceObject`, and the newly-
observed `KeSetAffinityThread` are left as generic offline stubs
unchanged -- none of the sampled call sites check their return values,
so their current no-op-with-a-status-code behavior does not block
anything further; extending the fix to them without evidence of a
similar blocking pattern would be an unverified guess this project's
discipline forbids.

## Tests

`tests/test_materialize_native_import_stubs.py`: new
`test_ob_reference_object_by_handle_writes_the_real_handle_through`
confirms the generated body writes through `r5` using `r3` and returns
`STATUS_SUCCESS`, with no `kOfflineStatus` reference. Full suite:
141/141 (was 140/140), 28/28 in this one file (was 27/27).

## Verified live: real behavioral change confirmed, same crash site persists

Rebuilt and reran the probe with `AC6_NATIVE_IMPORT_TRACE=1`.
`ObReferenceObjectByHandle` no longer appears as an unhandled import at
all -- confirming the fix is genuinely exercised. New, previously-unseen
imports are now reached downstream of it, most notably
`KeSetAffinityThread` (never observed in any prior trace across this
whole investigation, r130-r147). The `sub_821F7C80` crash still
reproduces at the identical site via the identical call chain (`gdb`
backtrace confirmed) -- expected and unsurprising, since that crash's own
causal chain (r130-r142) is a completely separate mechanism
(`sub_82338388`'s wait/stale-stack-read path) this fix does not touch.

## Decision

Kept and committed on its own merits, following the same precedent as
r145: two real, evidence-backed problems are fixed (35 calls' worth of
an unimplemented import, and a genuine thread-resume bug independent of
that import's own status code), the fix is minimal and directly
justified by every sampled call site's actual usage, and it measurably
changes observable guest behavior (new imports reached) even though it
does not resolve the specific crash this investigation continues to
track separately.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 141/141 (was 140/140). `git status`
(after `ctest`, not before): only the two intended source files plus the
pre-existing, unrelated `upstream/AC6_recomp` submodule pointer change.
`git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`:
185, unchanged.

## Next

1. `KeSetAffinityThread` is now reached for the first time in this whole
   investigation -- read its call site(s) before deciding whether it
   needs real handling, the same discipline this cycle and r146/r147
   applied.
2. Continue checking "does this fix change observable guest behavior
   anywhere" after any real native-runtime fix, independent of whether
   it resolves the crash currently under active investigation.
3. r144's determination about the `sub_821F7C80` crash chain's specific
   `[r1+88]` stack-content leaf and `IM_LOAD_IMMEDIATE` translation both
   still stand -- this cycle did not change either.
