# AC6 retail NTSC-U/J — real fix: `XamAlloc`/`XamFree` use the guest bump allocator (r195)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(182/182, up from 181/181).

## What was found

`DWORD XamAlloc(DWORD Type, SIZE_T Size, PVOID* pAddress)` and `DWORD
XamFree(PVOID pAddress)` are standard, documented XAM APIs. Unlike the
NT-family imports this project usually fixes, `XamAlloc`/`XamFree` return
a plain Win32-style `DWORD` status (`0` = `ERROR_SUCCESS`), not an
`NTSTATUS`.

This XEX has 3 real `XamAlloc` call sites and 4 real `XamFree` call sites.
One traced directly (`0x821fd440`, inside `Function_821FD3E8`, real range
`[0x821fd3e8, 0x821fd4bb]`): `r3=Type=0`, `r4=Size=0x440`, `r5=&pAddress`
(a stack out-slot). The caller treats the return as **signed** and
branches to its own error path only when negative (`or. r31,r3,r3; blt
...`). `kOfflineStatus` (`0xC00000BB`) is negative as a signed 32-bit
value, so every one of this XEX's 3 real `XamAlloc` call sites was a
guaranteed, deterministic allocation failure — not a status-shape
cosmetic difference, a real "every guest allocation through this API
always fails" bug.

## Fix

`XamAlloc` reuses the existing `allocate_guest(base, size)` bump
allocator (the same mechanism `ExAllocatePool`/`MmAllocatePhysicalMemoryEx`
already use), writes the resulting guest address through the real
`pAddress` out-pointer, and returns `0` (`ERROR_SUCCESS`) on success or
`0xE` (`ERROR_OUTOFMEMORY`) if the allocator itself returns `0`.
`XamFree` is a no-op returning `0`, the same "guest reservation lifetime
is owned by the bump allocator, never freed individually" precedent as
`ExFreePool`/`RtlFreeAnsiString`.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
182/182 (181/181 before this cycle, +1 new test,
`test_xam_alloc_and_free_use_the_guest_bump_allocator`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r194's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r195).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
