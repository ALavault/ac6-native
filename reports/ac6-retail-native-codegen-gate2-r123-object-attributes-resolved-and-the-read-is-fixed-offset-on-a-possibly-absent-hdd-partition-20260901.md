# AC6 retail NTSC-U/J — `OBJECT_ATTRIBUTES` fully resolved; the read is a fixed-offset query on a hard-drive partition this disc-based title may not have (r123)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle -- all
read-only via `DumpRange.java` and `FindDirectCallsTo.java`; one
throwaway boundary-lookup script created and deleted before this
report). XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** `git status` on
`recompilation/ace-combat-6-retail/` and `scripts/` confirmed clean
before and after (only the pre-existing, unrelated `upstream/AC6_recomp`
submodule pointer change).

## Continuing r122's frontier: `OBJECT_ATTRIBUTES`'s exact layout

r122 confirmed the register-argument layout for `NtCreateFile`/
`NtReadFile` directly from disassembly and found one field of
`ObjectAttributes` (`r5` in `Function_82390F48`) -- a real `ANSI_STRING`
pointing to a byte-verified static string
(`"\Device\Harddisk0\Partition1"`, `Length=28`) -- but left the
structure's remaining fields and the relationship to the per-call
sprintf'd filename buffer unresolved.

Re-reading the same disassembly (`generated` addresses
`0x82390f78`-`0x82390fbc`, already captured in r122's own dump) with
every `stw` mapped to its exact stack offset resolves the full
structure. `r5 = r1+0x78` (`ObjectAttributes`), and three consecutive
words are written there:

```
r1+0x78 (+0x0): 0                          -- RootDirectory
r1+0x7c (+0x4): 0x8201aa8c                 -- ObjectName -> ANSI_STRING*
r1+0x80 (+0x8): 0x40                       -- Attributes
```

**This is the well-documented, reduced Xbox 360 `OBJECT_ATTRIBUTES`
layout** (three fields, 12 bytes total -- smaller than desktop NT's
larger structure), confirmed here field-by-field from real bytes
rather than assumed: `RootDirectory` (0, i.e. absolute/no relative
handle), `ObjectName` (a pointer to an `ANSI_STRING`, not a
`UNICODE_STRING`), `Attributes` (`0x40` -- the standard, well-known NT
`OBJ_CASE_INSENSITIVE` flag value, external convention cited as such,
not sourced from this repo).

## `ObjectName` points directly at the static device string -- no per-call filename is involved

`ObjectName` (`0x8201aa8c`) is the *same* static `ANSI_STRING` r122
already byte-verified as `"\Device\Harddisk0\Partition1"`. **The
sprintf'd buffer built earlier in the function (`r1+0xb1`, with a
separate length field at `r1+0x54 = 0x22`) is never referenced by
`ObjectAttributes` at all.** `NtCreateFile` here opens the raw
partition device path itself, not a named file within it -- r122's
working assumption that this was a general "open an asset by path"
loader does not hold for this specific call site.

## The read is a fixed offset and fixed length, not path-driven

Re-confirming r122's own capture of the `NtReadFile` call
(`generated` `0x82391000`-`0x82391020`): `Length = 0x400` (1024 bytes),
`ByteOffset` points at a stack slot holding `0x800` (2048), both
compile-time constants, not derived from any argument. **This reads a
fixed 1KiB block at a fixed 2KiB offset from the raw partition
device**, consistent with querying a small, fixed system structure
(a partition table, volume descriptor, or similar) rather than loading
a named game asset.

## Traced the caller graph one level further

`FindDirectCallsTo.java` (read-only) against `Function_82390F48`
itself found exactly two direct callers, both inside
`Function_82391A40` (`0x82391a40`-`0x82391e7f` -- already identified in
r122 as one of `NtCreateFile`/`NtReadFile`'s own direct callers, so
this function both calls the wrapper *and* calls the imports directly a
second time). `Function_82391A40` itself has exactly one direct caller
(`0x82391eb8`), suggesting a small, largely self-contained cluster
around device/partition probing -- not yet connected to
`sub_821F4E70`'s indirect (`bctrl`-based) dispatch table, which
`FindDirectCallsTo.java` cannot see since it only matches direct `bl`
instructions.

## Decision

This changes the shape of the likely fix. A disc-based Ace Combat 6
retail title has no guaranteed reason to carry a populated
`\Device\Harddisk0\Partition1` -- reading a small fixed structure from
it looks like exactly the kind of optional hard-drive-cache probe real
hardware would expect to *fail cleanly* on a disc-only console (no HDD
installed, or an HDD present but without this title's cache
partition). If that is the real-hardware behavior, the correct fix is
very plausibly narrower than "implement a working file reader": return
a real NT failure status (e.g. `STATUS_OBJECT_NAME_NOT_FOUND` /
`STATUS_NO_SUCH_DEVICE`-shaped) from `NtCreateFile` instead of the
generic `kOfflineStatus`, so the guest's own already-written error path
runs as designed -- rather than building genuine partition-read
support this project has no evidence the retail flow actually needs.

This is not yet established as fact -- it is a concrete, evidence-
grounded hypothesis that reframes r121's "implement file I/O" framing
into "return the right failure code," which is a much smaller,
lower-risk change if verified. Guessing which specific NTSTATUS value
real hardware returns, or whether success is ever actually expected
here, is refused without further tracing.

## Gates

No native code changed. `git status` on
`recompilation/ace-combat-6-retail/`: clean, unchanged from before this
cycle. No throwaway Ghidra scripts left in `scripts/`.

## Next

1. Trace whether `Function_82391A40` (or its own single caller,
   `0x82391eb8`) is reached from `sub_821F4E70`'s indirect dispatch
   table -- `FindDirectCallsTo.java` cannot see indirect (`bctrl`)
   calls, so this needs either dumping the table's actual contents at
   its computed address, or a live instrumented capture of `ctr` at the
   `bctrl` site (the same technique r105-r121 have used throughout).
   This is the missing link between r117-r121's retry-loop mechanism
   and r122-r123's file-open findings -- currently two well-evidenced
   threads that plausibly connect but are not yet proven to.
2. If connected, test the "return a real failure status" hypothesis
   directly: implement `NtCreateFile` to return a plausible NT error
   (verify the exact value against an authoritative source first, per
   this project's standard) instead of `kOfflineStatus`, and check live
   whether that alone changes the retry loop's outcome -- a much
   smaller, more easily verified change than full file-read support.
3. If not connected, this file-open cluster may be unrelated to
   r105-r121's crash chain entirely, and the search for what actually
   writes the per-thread status field at `ctx.r13+336`'s indirection
   (r119's original, still-open question) needs to continue
   independently.
