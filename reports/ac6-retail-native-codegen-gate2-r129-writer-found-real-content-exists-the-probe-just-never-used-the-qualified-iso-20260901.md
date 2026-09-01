# AC6 retail NTSC-U/J — the writer of `0x8293b93c` is found: it is a real filename, `game:\DATA00.PAC`, and that file genuinely exists on this project's own qualified retail ISO (r129)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle** -- pure static
reading of already-generated source (a precise Python scan, not the
limited Ghidra materialization tool r128 found had blind spots for
this exact case), two read-only `DumpBytes.java` checks, one
`FindDirectCallsTo.java` check, and workspace-level file/hash checks
(`sha256sum`, `strings`) against files outside the git-tracked product
tree. `git status` on `recompilation/ace-combat-6-retail/` and
`scripts/` confirmed clean before and after (only the pre-existing,
unrelated `upstream/AC6_recomp` submodule pointer change).

## Continuing r128's frontier with a better instrument

r128 retracted a GDB-derived lead and left the writer of the handle
table entry at `0x8293b93c` unidentified, having exhausted
`FindPpcAddressMaterialization.java` (found nothing) and a hand grep
for direct `-18116`-displacement stores (found two, both against a
different base). This cycle wrote a precise Python scan across every
generated file for the specific two-instruction sequence
(`lis`-equivalent constant `-2104229888` followed within a few lines
by an `addi ..., -18116` forming a new register) -- a narrower,
custom check the general-purpose Ghidra tool apparently missed for
this specific case (three matches turned up, all real, none previously
found).

## The writer: `sub_821CC370`

All three matches trace to two functions: two are inside
`sub_821CC508` itself (the reader, already fully characterized by
r109-r121 -- reads, not writes), and **one is inside a different
function, `sub_821CC370`** (`generated/ppc_recomp.22.cpp` line 35196).
Reading it in full:

```cpp
PPC_FUNC_IMPL(__imp__sub_821CC370) {
  // r10 = 1st arg ("type"), then r3 = 2nd arg ("value")
  ...
  // (string-length / bounds check on the value, omitted here)
  r11 = 0x8293b93c;                    // table base -- CONFIRMED same address as r127
  r10 = (type & 0xFF) * 4;             // word index, matching r127's own indexing
  PPC_STORE_U32(r10 + r11, ctx.r3.u32); // table[type] = value
}
```

This is `sub_821CC370(type, value)` -- a general setter for exactly
the table `sub_821CC508`/`sub_821F4E70` read from (r127). Its only two
real callers (`FindDirectCallsTo.java`, read-only) are both inside
`Function_821D5F48` -- the same giant function this whole investigation
arc (r105-r128) has centered on -- called very early, well before
GATE1/GATE2:

```cpp
// generated/ppc_recomp.23.cpp lines 19009-19024
sub_821CC370(ctx, base);   // r3=0 (type), r4 = 0x82067d40 (value)  -> table[0]
sub_821CC370(ctx, base);   // r3=1 (type), r4 = 0x82067d54 (value)  -> table[1]
```

## `table[0]`'s value is a real filename, not a handle at all -- initially

`DumpBytes.java` (read-only) at `0x82067d40`:

```
67 61 6d 65 3a 5c 44 41 54 41 30 30 2e 50 41 43   "game:\DATA00.PAC"
```

`0x82067d54` (table[1]) continues with `"game:\DATA01..."`. **The
table starts out holding filename-template string pointers, one per
"type"** -- `game:` is the standard Xbox 360 drive-letter prefix for a
title's own mounted content, and `DATA00.PAC`/`DATA01.PAC` are
plausible asset-package files. `sub_821F4E70`'s caller (r127) reads
`table[0]` directly as a `FileHandle` for `NtReadFile` -- meaning some
code between this early string-table setup and the retry loop's first
run is expected to replace `table[0]`'s *string pointer* with a real
*file handle* (via its own `NtCreateFile("game:\DATA00.PAC", ...)`
call), or with `INVALID_HANDLE_VALUE` on failure. That intermediate
open-and-replace step was not traced to its own exact call site this
cycle, but the shape now fully explains r127's live capture
(`0xFFFFFFFF`, not the original string pointer) without needing to
find it: `INVALID_HANDLE_VALUE` is exactly what a failed
`NtCreateFile("game:\DATA00.PAC")` -- unimplemented, generic stub,
r121 -- would plausibly leave behind if that intermediate code
overwrites the slot on failure.

## The file genuinely exists -- this project's own qualified retail ISO has it

This session's entire investigation (r105-r128) ran the probe against
`build/.../codegen-20260831-mapfix-96838/assets/`, a directory
containing **only `default.xex`** -- confirmed by direct listing. No
`DATA00.PAC` could ever be found there regardless of what
`NtCreateFile` does. But this project's own target identity
(`recompilation/ace-combat-6-retail/targets/ntsc-uj.json`,
`ntsc-uj-campaign.json`) already cites a qualified retail ISO by SHA-256
(`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`).
That exact file exists at the workspace root
(`Ace Combat 6 - Fires of Liberation (USA, Japan) (En,Fr,De,Es,It).iso`)
-- `sha256sum` confirms an exact match. A raw `strings` scan of that
ISO finds both `DATA00.PAC` and `DATA01.PAC` present as literal
filenames.

**This is not a missing-content blocker.** The real game data this
whole chain has been chasing since r109 genuinely exists, in this
project's own already-qualified media -- it is simply media this
specific investigation's probe commands never pointed at, having used
the minimal XEX-only `assets/` directory throughout.

## Decision

This closes the practical question r122/r123/r127 all left open (is
there real content to read, or is this operation expected to fail
cleanly on real hardware): there is real content, on already-qualified
media this project already has. The correct fix path is now fully
determined in shape, even though not yet implemented: `NtCreateFile`
needs to translate `game:\`-prefixed guest paths against this
project's existing `read_xdvdfs_file` (`native/include/ac6/native_xdvdfs.h`)
when run against ISO media (as r108-r128's probe never was), and
`NtReadFile` needs to serve real bytes from the result -- exactly the
scope r122 originally deferred, now de-risked because the two open
questions blocking it (calling convention, and whether real content
exists at all) are both answered.

No native code changed this cycle. Implementing `NtCreateFile`/
`NtReadFile` against `read_xdvdfs_file` and re-running the probe
against the qualified ISO (not the `assets/` directory) is a
substantive, multi-piece change -- deserving its own dedicated cycle
with test coverage, per this project's own precedent (r108, r116),
not a rushed addition here.

## Gates

No native code changed. `git status` on
`recompilation/ace-combat-6-retail/`: clean, unchanged from before this
cycle.

## Next

1. Trace the still-unfound intermediate step: what code, between
   `sub_821CC370`'s early string-table setup and `sub_821CC508`'s
   retry loop, reads `table[0]`'s filename string, calls
   `NtCreateFile`, and writes the result (real handle or
   `INVALID_HANDLE_VALUE`) back into the same slot. Not required to
   implement the fix, but would close the one remaining gap in this
   cycle's otherwise-complete mechanism.
2. Implement `NtCreateFile` to translate a `game:\`-prefixed
   `ObjectAttributes.ObjectName` (r122/r123's already-resolved
   structure layout) into a `read_xdvdfs_file` call when the runtime
   was booted against ISO media; implement `NtReadFile` to serve real
   bytes from the result via the caller's `Buffer`/`Length`/`ByteOffset`
   (r122's confirmed calling convention), completing synchronously
   with a real `STATUS_SUCCESS` (or `STATUS_PENDING` then success on a
   second poll, matching r124's confirmed caller expectation) rather
   than the harness's current unconditional offline failure.
3. Re-run the probe against the qualified ISO
   (`Ace Combat 6 - Fires of Liberation (USA, Japan) (En,Fr,De,Es,It).iso`,
   confirmed by hash this cycle) instead of the `assets/` directory
   this whole investigation arc has used, once the above is
   implemented -- this is a prerequisite this cycle newly established,
   not optional.
4. Once implemented and re-verified live, this closes r100's original
   crash chain end to end -- from the very first cycle of this
   session's investigation to a concrete, evidence-complete fix.
