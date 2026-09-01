# AC6 retail NTSC-U/J — `NtCreateFile`/`NtReadFile`'s real PPC calling convention, verified from disassembly; implementation deferred pending `OBJECT_ATTRIBUTES` layout (r122)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle -- all
work read-only via `FindDirectCallsTo.java`, `DumpRange.java`,
`DumpBytes.java`, and one throwaway boundary-lookup script deleted
before this report), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** `git status` on
`recompilation/ace-combat-6-retail/` confirmed clean before and after
(only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change); `scripts/` confirmed clean of the one throwaway
lookup script used and removed this cycle.

## Continuing r121's frontier: implement `NtCreateFile`/`NtReadFile`

r121 named implementing these two imports as the next concrete step.
Before writing any stub, this cycle located and read every real call
site in the XEX, per this project's evidence discipline (never guess a
calling convention from an external spec alone when the actual call
site can be read).

## Real call sites found, and the calling convention verified directly

`FindDirectCallsTo.java` against the import-table addresses
(`0x823D022C` = `NtCreateFile`, `0x823D035C` = `NtReadFile`) found
seven direct calls, all clustered in one small region (`0x8239xxxx`),
not scattered through arbitrary game code -- consistent with a single
shared CRT-style file-loading helper, not many independent call sites.
The smallest function containing *both* calls together,
`Function_82390F48` (`0x82390f48`-`0x8239107b`, 307 bytes), was
disassembled in full (`DumpRange.java`, read-only).

**The register-argument layout for both imports is now confirmed
directly from real machine code**, not assumed from an external spec:

```
NtCreateFile(r3=&FileHandle, r4=DesiredAccess, r5=&ObjectAttributes,
             r6=&IoStatusBlock, r7=AllocationSize, r8=FileAttributes,
             r9=ShareAccess, r10=CreateDisposition)

NtReadFile(r3=FileHandle, r4=Event, r5=ApcRoutine, r6=ApcContext,
           r7=&IoStatusBlock, r8=Buffer, r9=Length, r10=&ByteOffset)
```

Both match the real Windows NT / Xbox 360 XDK signatures in argument
*order* and *count* exactly (9 real parameters each, with the 9th --
`CreateOptions` for `NtCreateFile`, `Key` for `NtReadFile` -- never
populated at this specific call site, so this guest caller only uses
the first 8 register-passed arguments -- consistent with every other
stub in `materialize_native_import_stubs.py` only ever reading
`ctx.r3`..`ctx.r10`). `CreateDisposition = 1` here, matching the
well-documented NT convention `FILE_OPEN` (open existing, fail if
absent) -- exactly right for a read-only asset loader, and consistent
across the whole 34-line disassembly this cycle read (build a path via
`bl 0x823830f0` (sprintf-shaped) into a stack buffer, `NtCreateFile`,
check the returned status for `< 0`, `NtReadFile` a fixed 0x400-byte
chunk at a fixed 0x800 byte offset into another stack buffer, then two
more calls to neighboring import addresses (`0x823d023c`, `0x823d024c`
-- unexamined this cycle, plausibly `NtReadFileScatter`-adjacent and
`NtClose`) before returning).

## What remains unresolved: `OBJECT_ATTRIBUTES`'s exact field layout

`ObjectAttributes` (`r5`) is built from two stack regions. One traces
to a genuine, real-NT-shaped `ANSI_STRING` (`{Length:u16,
MaximumLength:u16, Buffer:u32}`) pointing at a static string --
confirmed by direct byte read (`DumpBytes.java`, read-only) at the
constant address materialized right before the call
(`lis r11,-0x7dfe; subi r11,r11,0x5574` -> `0x8201aa8c`):

```
8201aa8c: 00 1c 00 1d 82 01 a9 fc   (Length=0x1c=28, MaxLength=0x1d=29, Buffer=0x8201a9fc)
```

`Length = 28` is not a coincidence: `"\Device\Harddisk0\Partition1"`
is exactly 28 characters -- a plausible, well-formed Xbox 360 device
path for the title's own game partition, strongly suggesting this
static `ANSI_STRING` is a `RootDirectory`-relative device prefix, with
the per-call filename (built separately via the earlier `sprintf`-style
call into a different stack buffer, `r1+0xb1`, bounded by
`li r5,0x3ff` = 1023 bytes) supplying the relative path within it.

**This is not yet enough to implement a correct guest-path ->
host-path translation.** Which stack offset is `RootDirectory` versus
`ObjectName`, and the exact `OBJECT_ATTRIBUTES` struct's field offsets
relative to `r5` itself, were not fully resolved this cycle -- only one
of the two name-related pieces (the static device-prefix `ANSI_STRING`)
was traced to a confirmed, byte-verified source.

## Decision

This is real, disassembly-verified groundwork -- the calling
convention for both imports is now known with high confidence, sourced
from the actual machine code rather than assumed from an external
spec (avoiding the exact gap r114/r115 flagged and later had to
caveat: "the real XDK signature... not sourced from a file in this
repo"). Implementing the stubs now, before the `OBJECT_ATTRIBUTES`
layout and the exact `AllocationSize`/`FileAttributes`/`ShareAccess`
bit meanings are fully resolved, risks the same pattern r115's own
report flagged in itself: a plausible-looking implementation that
turns out incomplete once tested, needing its own correction cycle.
Given this project's own precedent for pairing a fix's `case 3`-shaped
subtlety with a full trace before writing code (r108's
`MmQueryStatistics` fix followed exactly this pattern: trace first,
implement once every field the caller reads is accounted for), the
disciplined choice is to defer implementation one more cycle rather
than commit to a guessed struct layout now.

## Gates

No native code changed. `git status` on
`recompilation/ace-combat-6-retail/`: clean, unchanged from before this
cycle. No throwaway Ghidra scripts left in `scripts/`.

## Next

1. Resolve `OBJECT_ATTRIBUTES`'s exact field layout at `r5` -- read the
   instructions building the *second* half of the structure (the
   sprintf'd per-call filename's own `ANSI_STRING`-shaped header, and
   which offset within the 0x70-0x80 stack region holds
   `RootDirectory` versus `ObjectName`) before writing any stub.
2. Once resolved, implement `NtCreateFile`/`NtReadFile` in
   `tools/materialize_native_import_stubs.py` against this project's
   existing `read_xdvdfs_file`/media-reading infrastructure
   (`native/include/ac6/native_xdvdfs.h`), translating the guest's
   device-relative path into either an XDVDFS lookup (ISO mode) or a
   direct filesystem read (assets-directory mode) depending on
   `NativeRuntime`'s own `MediaInput::kind` -- check how `media_` is
   currently exposed (or not) to the import-stub layer before assuming
   it needs a new plumbing path.
3. Verify the exact bit meanings of the `DesiredAccess`/`FileAttributes`/
   `ShareAccess`/`CreateDisposition` constants this call site passes
   (`lis r4,-0x3ff0`, `li r8,4`, `li r9,1`, `li r10,1`) against an
   authoritative NT/XDK source before encoding any of them as asserted
   facts in a fix -- `CreateDisposition=1`/`FILE_OPEN` is a strong,
   well-known-convention match, but was not independently re-confirmed
   against a repo-local source this cycle.
