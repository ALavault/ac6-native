# AC6 retail NTSC-U/J — real fix: `RtlImageXexHeaderField` reports "not present" (r183)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(167/167, up from 166/166).

## Why this check was worth running

Continuing the offline-import sweep. `RtlImageXexHeaderField` was already
partially visible in earlier cycles' disassembly windows (r182's own
investigation, and the `XexGetModuleHandle`-adjacent thunk table) but had
not been checked directly. Its two real call sites both dereference the
return value, making it a real crash-risk category, not a cosmetic one.

## What was found

Real contract: `PVOID RtlImageXexHeaderField(PVOID XexHeaderBase, DWORD
ImageFlags)` — unlike almost every other import fixed so far, the
**return value itself is the field pointer** (`0` means "not present"),
not a status code at all.

**`0x821f7d88`** (queries field `0x20401`):

```
821f7d88  bl 0x823d03ac        ; RtlImageXexHeaderField(base, 0x20401)
821f7d8c  cmplwi r3,0x0
821f7d90  beq 0x821f7da8       ; not present -> a well-defined default path
821f7d94  lwz r30,0x0(r3)      ; present -> DEREFERENCES the returned pointer
```

**`0x82390e40`** (queries field `0x40006`):

```
82390e40  bl 0x823d03ac        ; RtlImageXexHeaderField(base, 0x40006)
82390e44  cmplwi r3,0x0
82390e48  stw r3,0x0(r31)      ; the raw return is stored as an output field's value
82390e4c  beq 0x82390e58       ; not present -> a real NT status (0xC0000225)
```

The generic offline-import fallback's `kOfflineStatus` (`0xC00000BB`) is
nonzero, so both call sites currently treat an unimplemented, non-existent
header field as "found" — the first would dereference `0xC00000BB` as a
guest pointer (a real crash risk if that address is unmapped, not a
cosmetic gap), the second would hand a garbage "field value" to its own
caller as if it were real header data.

## Fix

Returns `0` ("not present") unconditionally. This project has no
reachable evidence that either optional header field (`0x20401`,
`0x40006`) is actually present in this XEX's own header — no
header-field-table parser exists yet under `native/` to check — so the
honest, safe answer is "not present": both call sites' own
default-handling paths for that exact case are well-defined and clearly
intended as a real, exercised branch, not merely tolerated dead code.

## Gates

`ctest` 10/10 (native profile). Full retail-native pytest suite: 167/167
(166/166 before this cycle, +1 new test). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state.

## Next

1. If a future cycle needs one of these fields as *present* (rather than
   absent), a real XEX optional-header-table parser would need to be
   added under `native/` first — not undertaken here absent that need.
2. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181/r182/r183).
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
