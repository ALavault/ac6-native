# AC6 retail NTSC-U/J — real fix: `XamGetSystemVersion` stays below every observed threshold (r177)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9) and the full retail-native pytest suite
(160/160, up from 159/159).

## Why this check was worth running

r176 named `XamGetSystemVersion` as the next candidate: it gates the same
`0x821f4440` sign-in-resolution flow r176 just fixed, via a version-number
comparison read before the loop is ever reached.

## What was found

`XamGetSystemVersion`'s real contract is `DWORD XamGetSystemVersion(VOID)`
— a dashboard build number, not a status. Five real call sites read it and
compare it against a version threshold:

- `0x821fcd04` and `0x821fcef0`: compare `>= 0x200a3200` to decide whether
  to probe for an optional, newer-dashboard-only export via
  `XexGetModuleHandle`/`XexGetProcedureAddress` (both still on the generic
  offline fallback, and both already fail closed — `kOfflineStatus` reads
  as a negative signed value, so the probe safely bails either way).
- `0x82210ed4` and `0x82210fac`: compare a masked field against `0x8a100`
  to decide between a cached and an uncached path to the *same* underlying
  check function (`0x82210d60`) — a performance/compat shim, not a
  correctness fork.
- **`0x821f4440`** (r176's sign-in resolution helper): compares
  `>= 0x20096b00` and, if true, **skips the entire signin-state loop**
  r176 just fixed, falling through to a different, unexamined function
  instead:

```
821f4440  bl 0x823cfe9c        ; XamGetSystemVersion() -> r3
821f4444  lis r11,0x2009
821f4448  ori r11,r11,0x6b00
821f444c  cmplw cr6,r3,r11
821f4450  bge cr6,0x821f44a0   ; skip the signin loop entirely
```

Every examined branch at the other four call sites degrades to a working
path regardless of which side of its own threshold is taken. This one call
site is the exception: a value at or above `0x20096b00` would silently
defeat r176's own fix by never reaching the loop it corrected.

## Value chosen

`0x20000000` — a plausible, round dashboard-version-shaped value, not read
from this XEX's own bytes, and clearly below every threshold found
(`0x20096b00` is the lowest). Required for r176 to actually take effect at
`0x821f4440`, and confirmed safe at every other examined call site.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 160/160
(159/159 before this cycle, +1 new test). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state.

## Next

1. `XamInputGetState`/`XamInputSetState`/`XamInputGetCapabilities` (real
   controller I/O) remain the strongest still-unimplemented candidate for
   this project's own historically observed "contrôles nuls" symptom, but
   need a real native input backend that does not exist yet under
   `native/` — a materially larger task than this cycle's single-import
   fixes, still not undertaken.
2. Continue the broader sweep of the remaining offline-import fallback
   list (mirroring r90/r93/r164's own method) for further contract-shape
   candidates.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
