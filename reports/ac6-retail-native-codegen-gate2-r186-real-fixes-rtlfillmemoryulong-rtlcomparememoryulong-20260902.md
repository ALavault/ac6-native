# AC6 retail NTSC-U/J — real fixes: `RtlFillMemoryUlong`/`RtlCompareMemoryUlong` (r186)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with matching tests. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(172/172, up from 170/170).

## Why this cycle covers two imports together

r185 named both as standard NT RTL primitives with a single, fixed,
publicly documented algorithm each — real companions of each other (fill
vs. compare against a repeating 4-byte pattern), covered together as one
coherent pair, matching r170's and r185's own precedent.

## What was found

`RtlFillMemoryUlong`'s real signature is `VOID RtlFillMemoryUlong(PVOID
Destination, ULONG Length, ULONG Pattern)`. This XEX's one real call site
(`0x821f3354`) fills a local stack buffer (0x320 bytes) with pattern
`0x80000000`:

```
821f3348  lis r5,-0x8000       ; Pattern = 0x80000000
821f334c  li r4,0x320          ; Length = 0x320
821f3350  addi r3,r1,0xc0      ; Destination
821f3354  bl 0x823d03dc        ; RtlFillMemoryUlong(&buf, 0x320, 0x80000000)
```

The generic offline-import fallback never wrote through the pointer at
all, leaving whatever stack garbage was present instead of the intended
sentinel pattern.

`RtlCompareMemoryUlong`'s real signature is `ULONG
RtlCompareMemoryUlong(PVOID Source, ULONG Length, ULONG Pattern)` — the
real companion, comparing memory against a repeating pattern and returning
the byte count of the longest matching prefix.

## Fix

Both are implemented as their one fixed, standard algorithm (no ambiguity
to resolve, the same category as r184's SHA-1 and r185's calendar
conversion):

- `RtlFillMemoryUlong`: writes `Length / 4` `ULONG`s of `Pattern`.
- `RtlCompareMemoryUlong`: compares `Length / 4` `ULONG`s against
  `Pattern`, returning the byte offset of the first mismatch (or the full
  length if none).

## Gates

`ctest` 10/10 (native profile). Full retail-native pytest suite: 172/172
(170/170 before this cycle, +2 new tests). `git status` unchanged apart
from the intended change set and pre-existing, unrelated dirty state.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181-r186).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
