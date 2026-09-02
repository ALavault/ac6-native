# AC6 retail NTSC-U/J — real fix: `RtlUnicodeToMultiByteN` converts and succeeds (r187)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(173/173, up from 172/172).

## What was found

Real signature: `NTSTATUS RtlUnicodeToMultiByteN(PCHAR MultiByteString,
ULONG MaxBytesInMultiByteString, PULONG BytesInMultiByteString, PCWCH
UnicodeString, ULONG BytesInUnicodeString)`. This XEX's one real call site
(`0x821f4758`) confirms the argument shape (`r3`=dest, `r4`=maxDestBytes,
`r5`=`&bytesWritten` [`NULL` here], `r6`=source UTF-16, `r7`=source byte
length) and the real `NTSTATUS` success contract:

```
821f4758  bl 0x823d019c        ; RtlUnicodeToMultiByteN(...)
821f475c  cmpwi r3,0x0
821f4760  bge 0x821f472c       ; any non-negative return is success
821f4764  bl 0x823d018c        ; negative -> RtlNtStatusToDosError (r126)
```

Any non-negative return is success (matching real `NTSTATUS` convention),
not a discarded status — the generic offline fallback's `kOfflineStatus`
(`0xC00000BB`) is a genuinely negative `NTSTATUS`, so this call site
currently always took the failure branch into `RtlNtStatusToDosError`
(already implemented, r126) instead of converting anything.

## Fix

Converts each UTF-16 code unit to its low byte for codepoints `<= 0xFF` (a
real, standard Latin-1-shaped mapping) and the conventional `?` (`0x3F`)
replacement character for anything higher — ordinary NT
default-unmappable-character behavior, not a value chosen to force a
particular result. This project's own guest strings reaching this path
are file paths/titles, not general Unicode text, so this mapping covers
the realistic case exactly. Returns `STATUS_SUCCESS` (`0`) and, if
requested, the written byte count.

## Gates

`ctest` 10/10 (native profile). Full retail-native pytest suite: 173/173
(172/172 before this cycle, +1 new test). `git status` unchanged apart
from the intended change set and pre-existing, unrelated dirty state.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181-r187).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
