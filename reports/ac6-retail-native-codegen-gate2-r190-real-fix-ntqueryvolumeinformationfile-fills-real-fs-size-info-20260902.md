# AC6 retail NTSC-U/J — real fix: `NtQueryVolumeInformationFile` fills real FS size info (r190)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(177/177, up from 176/176).

## What was found

Real signature: `NTSTATUS NtQueryVolumeInformationFile(HANDLE FileHandle,
PIO_STATUS_BLOCK IoStatusBlock, PVOID FsInformation, ULONG Length,
FS_INFORMATION_CLASS FsInformationClass)` — a struct-fill through `r5`,
not a discarded status. All three of this XEX's real call sites pass
`FsInformationClass=3` (`FileFsSizeInformation`) and `Length=0x18` (24
bytes, exactly `sizeof(FILE_FS_SIZE_INFORMATION)`: two `LARGE_INTEGER`s
plus two `ULONG`s). One call site computes real free/total byte counts
from the queried fields and reports them to its own caller — a real
disk-space check:

```
823909b0  li r7,0x3            ; FsInformationClass = FileFsSizeInformation
823909b4  lwz r3,0x50(r1)      ; FileHandle
823909b8  li r6,0x18           ; Length = sizeof(FILE_FS_SIZE_INFORMATION)
823909bc  addi r5,r1,0x80      ; FsInformation output
823909c0  addi r4,r1,0x60      ; IoStatusBlock
823909c4  bl 0x823d032c        ; NtQueryVolumeInformationFile(...)
...
823909f8  mullw r11,r10,r11    ; SectorsPerAllocationUnit * BytesPerSector
82390a04  mulld r10,r10,r11    ; AvailableAllocationUnits * that -> free bytes
82390a08  mulld r11,r9,r11     ; TotalAllocationUnits * that -> total bytes
```

## Fix

Only `FileFsSizeInformation` is implemented; any other requested class
returns `STATUS_INVALID_INFO_CLASS` rather than a guessed struct shape
this XEX has no traced call site for.

Values: `SectorsPerAllocationUnit=0x20`, `BytesPerSector=0x200` — a
`0x4000` (16KiB) allocation unit, the real Xbox 360 FATX filesystem's own
documented default cluster size for large partitions, not invented for
this fix. Total/available space: 8 GiB, an ordinary generous default
(this project's own guest media is read-only and does not yet support
real writes, so "plenty of free space" avoids a false disk-full block
without asserting a specific real console's exact partition size).

A second call site compares the computed bytes-per-allocation-unit against
a caller-supplied expected value this cycle did not trace back to its
source; this fix cannot guarantee that comparison passes and does not
assert that it does — named honestly rather than claimed as fully
resolved.

## Gates

`ctest` 10/10 (native profile). Full retail-native pytest suite: 177/177
(176/176 before this cycle, +1 new test). `git status` unchanged apart
from the intended change set and pre-existing, unrelated dirty state.

## Next

1. If the second call site's validation (comparing computed bytes-per-unit
   against a caller-expected value) is later found to still fail, trace
   that expected value's own source before adjusting the constants here.
2. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181-r190).
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
