# AC6 retail NTSC-U/J — real fix: `XGetLanguage` returns English (r174)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9) and the full retail-native pytest suite
(158/158, up from 157/157).

## Why this check was worth running

r173 closed `XGetAVPack`, leaving `XGetLanguage` as the last unchecked
import in the platform-config family opened by r171.

## What was found

`XGetLanguage`'s one real call site, `0x821f5d9c`:

```
821f5d9c  bl 0x823d003c        ; XGetLanguage() -> r3
821f5da0  or r31,r3,r3
...
821f5dd4  cmplwi cr6,r31,0xa   ; bounds-check against 10
```

Unlike `XGetAVPack`, this return value is retained (`r31`), bounds-checked
against `10`, and (beyond this call site's own visible instructions) used
to index a per-language lookup table the surrounding function builds. The
exact value therefore matters here.

## Value chosen

`1` — the real Xbox 360 XDK's own standardized `XC_LANGUAGE_ENGLISH`
constant. Unlike a struct byte offset (compiler/layout-specific to this
XEX's own compiled binary, which is why this project's discipline refuses
assuming those from an external source), a platform enum constant like this
is a fixed, standardized protocol value defined by the XDK itself, not
something this XEX's own compilation could vary. `1` is safely within the
bounds-check (`<= 10`) and consistent with this project's own NTSC-U/J
target.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 158/158
(157/157 before this cycle, +1 new test). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state.

## Next

This closes the platform-config family opened by r171
(`XGetVideoMode`/`XGetGameRegion`/`XGetAVPack`/`XGetLanguage`) — no further
import in this family is currently named as unchecked. Remaining named,
unimplemented gaps:

1. `VdGetCurrentDisplayInformation` struct+0x05 (r170): a real boolean
   field, confirmed at two call sites, whose correct value is not yet
   traced.
2. A fresh, broader sweep of the offline-import fallback list (mirroring
   r90/r93/r164's own method) would be needed to name the next candidate
   once the above is resolved.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
