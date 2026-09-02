# AC6 retail NTSC-U/J — real fix: `KeQuerySystemTime` fills a real, changing FILETIME (r179)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9, this cycle's `std::chrono`-based fix also
compiles cleanly against the real toolchain) and the full retail-native
pytest suite (161/161, up from 160/160).

## Why this check was worth running

Continuing the broader offline-import sweep after r178's negative result,
`KeQuerySystemTime` stood out for having four real static call sites, all
of which read back the result and do real work with it — not a discarded
read, and a different shape of gap than the recent contract-shape fixes
(a struct-fill via pointer, the same gap category as `VdQueryVideoMode`
r168/r169).

## What was found

Real signature: `VOID KeQuerySystemTime(PLARGE_INTEGER SystemTime)` — a
single 64-bit FILETIME tick count (100ns units since 1601-01-01) written
through the pointer in `r3`, not a status return. The generic
offline-import fallback wrote nothing through that pointer at all. All
four of this XEX's own real call sites need the result to be a **real,
changing** value:

- `0x821f4bb4` and `0x821f5abc` feed the result straight into
  `RtlTimeToTimeFields` to populate a real year/month/day/hour/min/sec
  struct — a fixed small constant (e.g. leaving it near the FILETIME
  epoch) would render as a nonsensical date rather than a real one.
- `0x821f7f00` computes an elapsed-time delta between two saved
  timestamps to drive what reads as a real timer/animation gate — a fixed
  constant would freeze that delta at zero forever, and any logic waiting
  for real elapsed time to pass would never proceed.
- `0x82392cf8` takes the low 32 bits of the result as what reads as a
  session/seed value — a fixed constant would make it identical across
  runs instead of unique.

## Fix

Uses the host's own current wall-clock time
(`std::chrono::system_clock::now()`), converted to the Windows FILETIME
epoch (1601-01-01, an offset of `116444736000000000` in 100ns units — a
standard, well-known epoch-conversion constant, not read from this XEX's
own bytes and not invented for this fix), written through the pointer with
`PPC_STORE_U64`. This is exactly what real hardware provides — a real,
monotonically advancing clock — not a synthetic value chosen to force any
specific downstream comparison.

## Gates

`ctest` 9/9 (native profile), including a successful compile of the new
`<chrono>`-based code against the real `clang++-21` toolchain (added
`<ratio>` to this generated file's includes for the explicit
`std::ratio<1, 10000000>` duration cast). Full retail-native pytest suite:
161/161 (160/160 before this cycle, +1 new test). `git status` unchanged
apart from the intended change set and pre-existing, unrelated dirty
state.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179) for further contract-shape or struct-fill
   candidates not requiring new infrastructure.
2. The native controller input backend
   (`XamInputGetState`/`SetState`/`GetCapabilities`) still needs an
   explicit go/no-go scoping decision (r178) before any implementation
   begins.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
