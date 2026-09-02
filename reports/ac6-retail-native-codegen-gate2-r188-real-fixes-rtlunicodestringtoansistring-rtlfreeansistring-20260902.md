# AC6 retail NTSC-U/J — real fixes: `RtlUnicodeStringToAnsiString`/`RtlFreeAnsiString` (r188)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with matching tests. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(175/175, up from 173/173).

## Why this cycle covers two imports together

Real allocate/free companions of each other, discovered together at
adjacent addresses (`0x823927f0`/`0x8239281c`) in the same function, and
`RtlFreeAnsiString`'s only sensible implementation depends directly on
what `RtlUnicodeStringToAnsiString` allocates — covered as one pair,
matching r170/r185/r186's own precedent.

## What was found

`RtlUnicodeStringToAnsiString`'s real signature is `NTSTATUS
RtlUnicodeStringToAnsiString(PANSI_STRING DestinationString,
PCUNICODE_STRING SourceString, BOOLEAN AllocateDestinationString)`. This
project's own r122/r123 already confirmed the real `ANSI_STRING` layout
for this XEX (`{Length@0, MaximumLength@2, Buffer@4}`); `UNICODE_STRING`
is the same fixed, Microsoft-published shape with a `WCHAR*` buffer —
external protocol knowledge, not a guessed offset. This XEX's one real
call site (`0x823927f0`) confirms the argument shape
(`r3`=Destination, `r4`=Source, `r5`=`AllocateDestinationString`=`1`) and
the real `NTSTATUS` success contract (`>= 0` is success, matching r187's
own confirmed `RtlUnicodeToMultiByteN` contract); on success it later
calls `RtlFreeAnsiString` on the same destination, confirming a real
allocation is expected:

```
823927e4  li r5,0x1            ; AllocateDestinationString = TRUE
823927e8  addi r4,r1,0x58      ; SourceString (UNICODE_STRING)
823927ec  addi r3,r1,0x50      ; DestinationString (ANSI_STRING)
823927f0  bl 0x823d0b4c        ; RtlUnicodeStringToAnsiString
823927f4  or. r31,r3,r3
823927f8  bge 0x82392808       ; success
...
82392810  cmpwi cr6,r31,0x0
82392814  blt cr6,0x82392820   ; only free if the call succeeded
82392818  addi r3,r1,0x50
8239281c  bl 0x823d0b3c        ; RtlFreeAnsiString
```

## Fix

`RtlUnicodeStringToAnsiString` allocates a guest buffer via this file's
existing `allocate_guest` (the same helper `ExAllocatePool` already uses),
converts each UTF-16 code unit with the same low-byte/`?`-replacement
mapping r187 established, null-terminates it, and fills the `ANSI_STRING`
output fields. `AllocateDestinationString == 0` (caller-supplied buffer)
is also handled, respecting the destination's own `MaximumLength`, though
not exercised by this XEX's own traced call site.

`RtlFreeAnsiString` clears the `ANSI_STRING` fields rather than inventing
a per-allocation free: this file's own established `ExFreePool` precedent
is that guest pool pages are never individually reclaimed
(`allocate_guest`'s bump allocator has no free path), so a fake
per-allocation free would either be a no-op pretending otherwise or risk a
dangling-pointer bug this project's own allocator model does not support.

## Gates

`ctest` 10/10 (native profile). Full retail-native pytest suite: 175/175
(173/173 before this cycle, +2 new tests). `git status` unchanged apart
from the intended change set and pre-existing, unrelated dirty state.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181-r188).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
