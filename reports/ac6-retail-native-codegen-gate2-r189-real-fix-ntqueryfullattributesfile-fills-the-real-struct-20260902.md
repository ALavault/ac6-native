# AC6 retail NTSC-U/J — real fix: `NtQueryFullAttributesFile` fills the real struct (r189)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code + a small new accessor, with a matching test. Verified via
a clean `tools/build.py --target ntsc-uj --profile native` (`ctest`
10/10) and the full retail-native pytest suite (176/176, up from
175/175).

## What was found

Real signature: `NTSTATUS NtQueryFullAttributesFile(POBJECT_ATTRIBUTES
ObjectAttributes, PFILE_NETWORK_OPEN_INFORMATION FileInformation)` — a
struct-fill through `r4` plus a real `NTSTATUS` return, not a discarded
status. It reuses the exact `ObjectAttributes`/`ANSI_STRING`
path-extraction shape r122/r123 already confirmed for `NtCreateFile`
(`{RootDirectory@0, ObjectName(ptr)@4, Attributes@8}` /
`{Length@0, MaximumLength@2, Buffer@4}`).

Both of this XEX's real call sites confirm the standard
`FILE_NETWORK_OPEN_INFORMATION` layout (four `LARGE_INTEGER` timestamps,
then `AllocationSize`, `EndOfFile`, then a `ULONG` `FileAttributes` — 52
bytes) by reading `FileAttributes` at struct+`0x30`:

```
82392550  bl 0x823d0b2c        ; NtQueryFullAttributesFile(&ObjAttr, &r1+0x70)
82392554  cmpwi r3,0x0
82392558  blt 0x82392564       ; negative -> failure path
8239255c  lwz r3,0xa0(r1)      ; r1+0x70 struct + 0x30 = FileAttributes
```

matching the real, Microsoft-published offset exactly. The second call
site reads all seven fields.

## Fix

Matches `NtCreateFile`'s own established discipline: a real file that
does not exist in the bound media is a normal, expected
`STATUS_OBJECT_NAME_NOT_FOUND`, not a fabricated error — it opens whatever
real media the runtime was actually booted against
(`native_guest_media_service()`), reads its real size, and closes it
(this call never returns a handle). Adds
`NativeGuestInputService`... no — adds
`NativeGuestMediaService::file_size()` (`native_guest_media.h`), a small
accessor on the *existing* media service, not a new subsystem: no import
needed a file's real size without a full open/read/close cycle until now.
Timestamps are left at `0` — no call site reads them, matching this
project's own established discipline of not asserting unread fields
(r169). `FileAttributes` is set to `FILE_ATTRIBUTE_NORMAL` (`0x80`) for
any file found, since this project's media abstraction does not currently
distinguish directories from files.

## Gates

`ctest` 10/10 (native profile, confirming the new accessor compiles and
links). Full retail-native pytest suite: 176/176 (175/175 before this
cycle, +1 new test). `git status` unchanged apart from the intended
change set and pre-existing, unrelated dirty state.

## Next

1. `NtQueryVolumeInformationFile` (3 real call sites) is a plausible
   next candidate now that `file_size()` exists.
2. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181-r189).
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
