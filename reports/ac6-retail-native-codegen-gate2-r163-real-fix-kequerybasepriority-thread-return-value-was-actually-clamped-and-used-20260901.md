# AC6 retail NTSC-U/J — real fix: `KeQueryBasePriorityThread` returned an NTSTATUS-shaped status code that a real caller actually clamps and uses (r163)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, `-readOnly -noanalysis`. XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target
ntsc-uj --profile native` (`ctest` 9/9), the full retail-native pytest
suite (145/145, up from 144/144), and a live import trace confirming the
import no longer reaches the generic offline-import fallback.

## Why this check was worth running

r162 named `KeQueryBasePriorityThread` as the direct next candidate in the
same import family as `ObDereferenceObject`/`KeSetBasePriorityThread`
(both fixed r162) — not yet checked for the same NTSTATUS-shape bug.

## What was found: unlike its siblings, this one's return value is actually used

`Ac6Xrefs.java` against this import's real thunk address (`0x823D010C`)
finds exactly one static call site in the whole XEX, `sub_821F3EA0` (the
same function whose disassembly r162 already read in full while checking
`ObDereferenceObject`'s call sites). Unlike every `ObDereferenceObject`/
`KeSetBasePriorityThread` call site (all of which discard the return
value), this one **does not**:

```
821f3e48  lwz r3,0x50(r1)
821f3e4c  bl 0x823d010c            ; KeQueryBasePriorityThread(handle)
821f3e50  or r31,r3,r3             ; r31 = the return value
821f3e54  cmpwi cr6,r31,0x10
821f3e58  bne cr6,0x821f3e64
821f3e5c  li r31,0xf               ; clamp to +15 if > 16
821f3e60  b 0x821f3e70
821f3e64  cmpwi cr6,r31,-0x10
821f3e68  bne cr6,0x821f3e70
821f3e6c  li r31,-0xf              ; clamp to -15 if < -16
821f3e70  lwz r3,0x50(r1)
821f3e74  bl 0x823d00dc            ; ObDereferenceObject(handle)
821f3e78  or r3,r31,r31            ; return the (possibly clamped) r31
821f3e7c  b 0x821f3e8c
```

The generic offline-import fallback this call previously fell through to
returns `kOfflineStatus` (`0xC00000BB`) — as a signed `LONG`, a large
negative value (`-1073741381`), far below `-16`. **This always triggered
the `< -0x10` clamp branch**, meaning every real query of this thread's
base priority previously reported the clamp floor (`-15`) unconditionally,
regardless of what a genuine priority value would have been — a real,
observable consequence of the wrong contract shape, not merely a latent
one (contrast with r162's two fixes, where no traced caller used the value
at all).

## Fix

`KeQueryBasePriorityThread(PKTHREAD Thread) -> LONG` (current base
priority increment) is not an NTSTATUS. This project tracks no real
per-thread priority state (matching the "single host-scheduled execution
model" already established for the sibling `KeSetAffinityThread`/
`KeSetBasePriorityThread` stubs), so `0` (baseline/normal priority) is the
safe, in-range default — it keeps the caller's own clamp a no-op instead
of always firing, which is the most faithful behavior available without
inventing per-thread state this project does not model.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 145/145
(144/144 before this cycle, +1 new test:
`test_ke_query_base_priority_thread_returns_an_in_range_value_not_a_status`).
Live import trace confirms the import no longer reaches
`trace_offline_import()`. `gdb` backtrace confirms the tracked
`sub_821F7C80` crash still reproduces identically (unrelated chain).
`git status` clean apart from the intended change set.

## Next

Both of Gate 2's named frontiers are unchanged: DATA.TBL chain fully
traced and closed (r144/r153/r161); `IM_LOAD_IMMEDIATE`→SPIR-V
policy-blocked pending Xenos fetch-signature qualification. No further
named candidate remains in the `ObDereferenceObject`/`KeSetBasePriorityThread`/
`KeQueryBasePriorityThread` family — all three are now fixed. A fresh scan
of this build's own live import trace (`AC6_NATIVE_IMPORT_TRACE=1`) for any
other still-unaddressed high-call-volume offline-import stub would be the
natural next check if this thread continues.
