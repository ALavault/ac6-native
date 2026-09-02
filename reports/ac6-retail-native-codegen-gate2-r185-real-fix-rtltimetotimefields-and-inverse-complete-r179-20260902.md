# AC6 retail NTSC-U/J — real fix: `RtlTimeToTimeFields`/`RtlTimeFieldsToTime` complete r179's calendar conversion (r185)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with matching tests. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10, confirming the new C++20 `<chrono>`
calendar code compiles against the real `clang++-21` toolchain) and the
full retail-native pytest suite (170/170, up from 168/168).

## Why this cycle covers two imports together

Both are the real, standard-library-defined inverse of each other, use the
identical `TIME_FIELDS` struct offsets (confirmed at both of their own
real call sites), and were discovered and verified together as one
coherent round-trip — matching r170's own precedent of covering a
confirmed family in one cycle rather than splitting evidence that was
derived jointly.

## What was found

`RtlTimeToTimeFields`'s real signature is `VOID
RtlTimeToTimeFields(PLARGE_INTEGER Time, PTIME_FIELDS TimeFields)`. Two of
r179's own four `KeQuerySystemTime` call sites (`0x821f4bb4`,
`0x821f5abc`) feed its result straight into this function to populate a
real calendar struct — r179's own fix was still incomplete on its own: the
FILETIME it computed was being handed to a no-op that never wrote
`TimeFields` at all, so the calendar struct stayed uninitialized
regardless of `KeQuerySystemTime` being correct.

Both of this XEX's own real call sites confirm the standard Win32
`TIME_FIELDS` layout byte-for-byte: struct+`0x0`/`0x2`/`0x4`/`0x6`/`0x8`/
`0xa`/`0xc`/`0xe` = Year/Month/Day/Hour/Minute/Second/Millisecond/Weekday
(all 16-bit fields).

`RtlTimeFieldsToTime`'s real signature is `BOOLEAN
RtlTimeFieldsToTime(PTIME_FIELDS TimeFields, PLARGE_INTEGER Time)` — the
real inverse. Its one real call site (`0x821fb3a4`) confirms the same
field offsets (writing them just before the call) and the return
contract: `rlwinm. r11,r3,0,0x18,0x1f; beq <treat as failure>` — the low
byte of the return is the real `BOOLEAN` (nonzero = valid fields), matching
the documented XDK contract exactly.

## Fix

Both use C++20 `<chrono>`'s own calendar facilities
(`std::chrono::year_month_day`, `std::chrono::weekday`,
`std::chrono::hh_mm_ss`) rather than a hand-rolled Gregorian-calendar
reimplementation — the real, standard algorithm, verified to compile and
run correctly against this toolchain before wiring it in (a standalone
`clang++-21 -std=c++20` smoke test converting a known FILETIME value
produced the expected calendar date). Like r184's SHA-1, this is a case
where "the real value" has no ambiguity: the conversion is deterministic
given the input.

## Gates

`ctest` 10/10 (native profile, including a successful compile of the new
`<chrono>` calendar code). Full retail-native pytest suite: 170/170
(168/168 before this cycle, +2 new tests). `git status` unchanged apart
from the intended change set and pre-existing, unrelated dirty state.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181-r185). `RtlCompareMemoryUlong` (7 real
   call sites) and `RtlFillMemoryUlong` (1) are standard, well-defined NT
   RTL primitives not yet checked — likely similarly low-ambiguity fixes.
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
