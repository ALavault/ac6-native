# AC6 retail NTSC-U/J — the uninitialized stack read is pinpointed to a guest address outside even `_xstart`'s own frame (r106)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed.** Same technique as r105:
temporary `fprintf` diagnostics in the generated (never hand-maintained,
gitignored) `ppc_recomp.23.cpp`, one native run, reverted from backup
before this report. `ctest` (9/9) reconfirmed clean afterward; `git
status` on `native/` shows nothing changed.

## Continuing r105's frontier

r105 found that `Function_821D5F48` reads a local stack slot
(`ctx.r1.u32 + 108`) that nothing in its own body or its sole caller
(`sub_821D7DE0`) ever writes, and left open whether the expected value
comes from a fuller real-hardware boot sequence or is a genuine
probe-harness gap.

## Pinpointing the exact address and value directly, not by hand arithmetic

Added one `fprintf` at the read site itself, printing the resolved guest
address, the value read, and `ctx.r1.u32` for cross-checking — removing
any risk of the kind of by-hand frame-arithmetic error this cycle
initially made and caught before it went further (see below). One native
run, no GDB:

```
[r106] Function_821D5F48 read stack slot guest_addr=0x8feffd1c value=0x400000 (ctx.r1.u32=0x8feffcb0)
```

This matches hand-computed frame arithmetic exactly, once done correctly:
harness sets the probe thread's initial `r1 = 0x8ff00000`; `_xstart`'s own
prologue (`stwu r1,-0x1f0(r1)`) reduces it to `0x8feffe10`; `sub_821D7DE0`
(`stwu r1,-0x70(r1)`) to `0x8feffda0`; `Function_821D5F48`
(`stwu r1,-0xf0(r1)`) to `0x8feffcb0` (confirmed live, matches
`ctx.r1.u32` above exactly); `+108` gives `0x8feffd1c` (confirmed live,
matches the printed address exactly).

**This address is below `0x8feffe10` — outside `_xstart`'s own 496-byte
frame, not just outside `Function_821D5F48`'s and `sub_821D7DE0`'s.**
Since `_xstart` is the XEX entry point (no guest caller), nothing in the
traced PPC call chain owns this memory at all.

## A self-caught arithmetic mistake, corrected before it misled the report

The first version of this address computation (done by hand, before the
above direct instrumentation) was off by 496 bytes — it treated the
harness's *initial* `r1` (`0x8ff00000`) as the value already inside
`_xstart`'s reduced frame, forgetting `_xstart`'s own `stwu` first. That
wrong address (`0x8fefff0c`) was checked against the r100/r101 crash's
`apport` core dump and read as zero, which briefly looked like it
contradicted this cycle's live `0x400000` reading. Recomputing carefully
and re-reading the *correct* address (`0x8feffd1c`) in the same core dump
also shows zero — consistent, not contradictory: the core dump captures
memory state at the much later `sub_821D6C20` crash, after this stack
region has almost certainly been reused and overwritten by everything
that ran between `Function_821D5F48` returning and the eventual crash.
The two readings are at different points in time, not different runs of
the same moment — there is no evidence here of the guest memory itself
being non-deterministic (`GuestAddressSpace` uses a plain
`MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE` mapping, which Linux
guarantees is zero-filled on first touch, checked directly in
`native_guest_memory.cpp`).

## Decision

Still not implementing a fix. What's now precisely established: a
specific guest address (`0x8feffd1c`), reliably holding `0x400000` at the
moment it's read, lying outside every guest stack frame this project has
traced back to and including the XEX entry point itself. The value must
be written by something with a call/stack history this project has not
yet reached — plausibly action taken before `_xstart` is entered at all
(kernel/loader-level XEX bring-up on real hardware, not currently
replicated by this harness's `initialize_probe_thread`), or genuinely
unclaimed memory whose expected content this project cannot determine
without tracing that earlier history. Guessing a value to write there
would be exactly the kind of unverified rule this project's evidence
discipline refuses.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`native/`: clean.

## Next

1. This specific value's true origin requires tracing what a fuller boot
   sequence (kernel-level XEX loader activity before `_xstart`, which
   this native harness does not currently model at all) would place at
   this exact stack address — a substantially different, larger-scoped
   investigation than anything attempted in r100-r106, likely requiring
   either external documentation of the Xenon kernel's thread/stack
   bring-up conventions or accepting this as outside what static
   disassembly of the title alone can resolve.
2. Given the address falls outside the guest call graph entirely, the
   pragmatic path forward is probably not "trace further" but "scope
   this as a probe-harness limitation" per r105's second option: the
   entry-probe's `initialize_probe_thread` sets up a minimal PCR/TLS pair
   and a bare stack pointer, not a full kernel-equivalent bring-up. A
   future cycle could investigate what a more complete thread
   initialization would need to provide, bounded by evidence rather than
   guessed constants.
3. r100's older open thread (the `sub_821E6AC8`/`sub_821F03B0` wait chain,
   r94) stays probably moot for the same reason r101-r105 gave.
