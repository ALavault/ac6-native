# AC6 retail NTSC-U/J — the retry loop is genuinely waiting on file I/O; `NtReadFile`/`NtCreateFile` are unimplemented and never update the status field it polls (r121)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** Two rounds of
`AC6_R121_DIAG`-gated instrumentation were added to and fully reverted
from the generated (gitignored, untracked) `ppc_recomp.22.cpp` --
confirmed via `grep -c "r121"` returning 0 and `ctest` 9/9 passing
afterward. A final check used only the already-existing, permanent
`AC6_NATIVE_IMPORT_TRACE` mechanism, no new instrumentation.

## Continuing r119/r120's frontier, and a self-correction found along the way

r119 named instrumenting `ctx.r13` and the retry counter at `+22896`
as the concrete next step. Instrumenting both in one run:

```
[r121] status check: r30=0 r3(status)=3221225659 ctx.r13=0x0f000000 r13+336=0x00000000 ...
[r121] retry counter (+22896) = 5   (then 4, 3, 2, 1, 0 -- exhausts and fails, deterministically)
```

The status value `3221225659` = `0xC00000BB`.

**Correcting a real error in r117 and r119**: both reports described
`sub_821F75F0`'s branch (`generated/ppc_recomp.27.cpp` line 8330) as
"if the field at `r13+336` is nonzero, return the indirected value;
else return 0." Re-reading it while building this cycle's
instrumentation shows this is backwards: `cmplwi cr6,r11,0; bne
cr6,loc_821F7608` branches to the **zero-return** path when the field
is *nonzero*, and falls through to the indirected read when it is
*zero*. The first instrumentation round (gating the indirected read on
`f336 != 0`) reproduced this exact error and printed a placeholder
sentinel instead of a real value -- caught immediately from the
placeholder appearing in the output, fixed, and rebuilt within the same
cycle rather than reported as a result.

## The corrected trace: a real value, chased to its source

With the condition fixed (`f336 == 0` triggers the indirected read,
matching the guest code exactly):

```
[r121] status check: ... r13+336=0x00000000 p256=0x0f001000 indirected=0xc00000bb
```

`p256` (`PPC_LOAD_U32(r13+256)`) is `0x0f001000`. `indirected`
(`PPC_LOAD_U32(p256+352)`, i.e. guest address `0x0f001160`) is
`0xC00000BB`.

**`0xC00000BB` is `kOfflineStatus`** -- the exact literal this
project's own `tools/materialize_native_import_stubs.py` defines
(`constexpr std::uint64_t kOfflineStatus = 0xC00000BBull;`) and returns
in `ctx.r3` from the generic fallback for *any* import with no
dedicated stub. It cannot appear in guest memory on its own; the
generic fallback only ever sets a register, never writes to guest
memory (`ctx.r3.u64 = kOfflineStatus;`, nothing else). Its presence at
`0x0f001160` means guest code itself copied an unimplemented import's
return value into this status field at some earlier point.

## Where `ctx.r13` and this field actually come from

`ctx.r13 = 0x0f000000` is not guest state at all -- it is
`kProbePcrAddress`, a deliberate, hand-maintained constant this
project's own harness sets
(`native/src/ac6recomp_main.cpp` lines 17-30), part of a minimal
Processor Control Region emulation that explicitly initializes only a
handful of offsets (`+0x00`, `+0x30`, `+0x100` = `kProbeThreadAddress`,
`+0x2a8`, `+0x10c`, `+0x70`, `+0x74`). `kProbeThreadAddress + 0x160`
(`0x0f001160`, matching `p256 + 352` above) is **not one of the fields
this minimal PCR setup ever writes** -- whatever value sits there comes
entirely from guest code, not from the harness's own initialization.

## Confirmed: `NtReadFile`/`NtCreateFile` are unimplemented

A run with the pre-existing, permanent `AC6_NATIVE_IMPORT_TRACE`
mechanism (no new instrumentation) listed every unimplemented import
reached before the crash:

```
     43 RtlNtStatusToDosError
     35 ObReferenceObjectByHandle
      6 NtReadFile
      4 RtlTryEnterCriticalSection
      4 NtCreateFile
      4 KeLeaveCriticalRegion
      ...
```

**`NtReadFile` and `NtCreateFile` -- the canonical Xbox 360/Win32 async
file I/O pair -- both fall through to the generic offline stub.**
`NtReadFile`'s real contract writes completion status into a caller-
supplied `IO_STATUS_BLOCK`; the generic fallback never touches guest
memory at all. This matches every piece of this investigation arc
precisely: the `997`/`ERROR_IO_PENDING`-shaped checks r109/r117/r119
all found, the retry-then-give-up shape r119 traced, and now a status
field that holds exactly the "no stub implemented" sentinel instead of
a real I/O completion code -- because the operation that was supposed
to update it (a file read, almost certainly loading a game asset from
`assets/`) never ran any real logic at all.

## Decision

This closes the mechanistic chain this arc has built cycle over cycle
(r105 through r120): the retry loop is not failing on a harness
timing/threading quirk -- it is genuinely, correctly waiting for a file
read that this harness has never implemented. `NtCreateFile`/
`NtReadFile` are real gaps, not stubs deliberately left generic for a
good reason (unlike, say, `RtlTryEnterCriticalSection` or
`KeEnterCriticalRegion`/`KeLeaveCriticalRegion`, which appear in the
same list and are far more plausible to leave as no-ops for an
offline, single-process harness).

No native code changed this cycle. Implementing `NtCreateFile`/
`NtReadFile` against this harness's existing asset-reading
infrastructure (this project already has `native_xdvdfs`/media-reading
capability per its own test suite) is a substantive, focused change --
in the spirit of r108's `MmQueryStatistics` fix -- that deserves its
own cycle with dedicated verification, not a rushed addition here.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`recompilation/ace-combat-6-retail/`: clean (both instrumentation
rounds fully reverted before this report, confirmed via `grep -c
"r121"` returning 0).

## Next

1. Implement `NtCreateFile` and `NtReadFile` against this project's
   existing native media/XDVDFS reading capability
   (`tools/materialize_native_import_stubs.py`), writing a real,
   synchronous completion status into the caller's `IO_STATUS_BLOCK`
   rather than falling through to the generic offline stub -- check
   this project's own `native_xdvdfs`/media source and tests first for
   what read primitive already exists before writing a new one.
2. Verify the real Xbox 360 `NtReadFile`/`NtCreateFile` PPC calling
   convention (which register carries the `IO_STATUS_BLOCK` pointer,
   what fields it has) against an authoritative source before
   implementing -- not yet confirmed this cycle.
3. Once implemented, re-verify live (the crash is deterministic per
   r116) that the retry loop's status check now succeeds, `0x82935d98`
   gets written (r117's finding), and `sub_821D6C20` no longer
   dereferences a null vtable pointer.
