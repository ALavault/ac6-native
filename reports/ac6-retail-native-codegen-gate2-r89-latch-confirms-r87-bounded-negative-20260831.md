# AC6 retail NTSC-U/J — the completion latch confirms r87/r88 by live evidence; bounded negative for this sub-thread (r89)

Date: 2026-08-31.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. No native code changed in the committed tree (the single-thread probe
patch to `materialize_native_import_stubs.py` was applied, used, and reverted
via `git checkout --`, then the build-tree stub file was regenerated and
`ac6recomp` rebuilt to a clean, unmodified state — confirmed by `git status`
after `ctest`).

## Method

Per r88's own recommendation ("the next cycle should reconsider scope rather
than propose a fifth specific hypothesis"), this cycle did two bounded,
already-planned reads rather than opening a new mechanism:

1. **Static**: `FindStoresAtDisplacement.java 0x2abd` against `ac6-us`,
   listing every store to `object+0x2abd` anywhere in the retail image.
2. **Live**: single-thread probe
   (`AC6_NATIVE_EXPERIMENT_SINGLE_THREAD=1 AC6_NATIVE_ALLOW_ENTRY_PROBE=1`),
   breaking at the true raw entry of `__imp__sub_821E64A8` to capture the
   base pointer (`$rsi`) before any register reallocation (the r85-verified
   method), then interrupting mid-stall (confirmed stopped inside
   `__imp__sub_821E6AC8`, matching every prior cycle's report of where this
   thread parks) and reading, byte-by-byte (`x/Nxb`, no endianness
   ambiguity), three fields on the confirmed object `0x10001a00`:
   `object+0x2abd` (1 byte), `object+0x540c` (4 bytes), and
   `*(object+0x2a90's pointee)+0x0` = `*(0x164e0000)+0x0` (4 bytes) — the
   exact field r86 established `sub_821E61A8` dereferences (distinct from
   the `+0x3c` field `drain_locked()` writes).

## Static finding: `sub_821E60A8` writes its own completion latch

The `0x2abd` scan found one hit inside `sub_821E60A8` itself
(`821e60a8 821e6184 stb r11,0x2abd(r31)`). Full disassembly of
`sub_821E60A8` (`0x821e60a8`-`0x821e61a0`, verified complete — the function
ends in `blr` with a matched prologue/epilogue) shows:

- `0x821e6134`: unconditional call to `sub_821E5E48(object)`, reached
  regardless of the branches above it.
- `0x821e613c-0x6160`: three sequential gates, each `bne`/`beq` to the
  function's tail (`0x821e6188`, skipping everything below) if it fails:
  (a) `object+0x2abc` bit `0x80` clear; (b) a global flag (`lis/lwz` at a
  fixed host-relative slot) nonzero; (c) `object+0x2abd` bit `0x2` clear.
- Only if all three gates pass: `0x6164-0x6178` reads `object+0x2a9c` (the
  "limit", r85: `7` on this object) and, unless it equals exactly `2`, calls
  `sub_821E61A8(object, limit-2, 0)` directly — **`sub_821E60A8` itself
  invokes the same wait/poll function the guest thread blocks in**, not just
  the previously-known `sub_821E5D60` unblock write.
- `0x821e617c-0x6184`: unconditionally (once the three gates passed and the
  optional call above completed), sets bit `0x2` of `object+0x2abd` and
  writes it back. This is a **one-shot completion latch**: once set, every
  future call to `sub_821E60A8` for this object fails gate (c) and skips
  the entire block above, including the `sub_821E61A8` call.

This was not previously documented — r87/r88 traced the *interrupt-callback*
route to `sub_821E60A8`/`sub_821E5D60` but did not disassemble
`sub_821E60A8`'s own body far enough to find this second, self-contained
gate/latch/recheck structure.

## Live finding: the latch has never fired for this object

- `object+0x2abd` = **`0x00`** — bit `0x2` clear. Per the static finding,
  this means `sub_821E60A8` has never reached its own tail write for this
  object: either it has never been called for `0x10001a00` at all, or every
  call so far failed one of the three gates before reaching the latch.
- `object+0x540c` = **`0x00 0x00 0x00 0x00`** — all zero, stable at the
  moment read. (This field's role was not independently established this
  cycle; it was read only because r79/prior analysis had flagged it as a
  candidate gate input elsewhere in the allocator family. No claim is made
  about what writes it or what it gates — that would need its own static
  trace, not undertaken here per the bounded-scope decision below.)
- `*(0x164e0000)+0x0` = raw bytes `0x05 0x00 0x00 0x00` (ascending address
  order). **Not interpreted as a number in this report.** `store_guest_word`
  (`native_guest_vd.cpp:33-37`) always byte-swaps via `__builtin_bswap32`
  before the raw `memcpy`, so a native write of decimal `5` would leave
  bytes `00 00 00 05` at this address — the *opposite* byte order from what
  was read. Nothing in `native_guest_vd.cpp` writes to this exact offset
  (`drain_locked()` writes `readback_+0x3c`, not `readback_+0x0`, per r86);
  its writer — guest PPC code, or leftover allocator content — is not
  identified. Asserting either the big-endian (`0x05000000`) or
  little-endian (`5`) reading without finding the actual writer would be
  exactly the kind of inferred value the project's evidence discipline
  refuses. Left open, explicitly.

## Decision

This cycle's two reads corroborate r87/r88's already-established root cause
by an independent, live-memory route: the completion latch that
`sub_821E60A8` is supposed to set for this object is provably still clear,
consistent with — not merely assumed from — the interrupt callback chain
that would normally trigger it never firing (native `VdSetGraphicsInterruptCallback`
stub is a no-op, r87). This is confirmation, not a new unblocking mechanism:
it does not, by itself, name a different caller of `sub_821E60A8` for this
object, and per the advisor-reviewed decision rule for this cycle
("if the reads come back uninformative [for a *new* mechanism], write the
bounded negative and return to NEXT.md's list"), that is the outcome here.

**Bounded negative for this sub-thread** (`sub_821E6AC8`-parked guest thread,
`0x10001a00`): five cycles (r85 object-identity correction, r86 pipeline-vs-wait
separation, r87 interrupt-callback lead, r88 refutation of that lead's specific
mechanism, r89 this cycle's live confirmation) have established, with concrete
addresses and either static or live evidence for each claim:

- the correct object (`0x10001a00`, confirmed 4 separate ways now);
- the PM4/Vd pipeline itself works and is not the blocker;
- the wait is a real, correctly-initialized poll on a generic
  76-call-site sub-allocator, not a corrupted or premature gate;
- the one identified unblock path (interrupt callback →
  `sub_821E60A8`/`sub_821E5D60`) is real in the disassembly but never
  reachable in the native runtime, confirmed by both static tracing (r87/r88)
  and now live memory state (r89, this cycle) — the completion latch it
  would set has never been set.

What remains **not established**, stated plainly: whether any *other*, not
yet identified, guest code path calls `sub_821E60A8` (or otherwise clears
the wait) for this specific object outside the interrupt-callback route;
what `object+0x540c` and `*(0x164e0000)+0x0` actually represent and who
writes them. Per r88's own scope call (endorsed this cycle rather than
opened as a fifth hypothesis), the substantially more expensive static pass
that could answer this — cross-referencing all 76 call sites of
`sub_821E60A8` — is not undertaken here. This sub-thread is closed for now
at this bounded negative; work returns to NEXT.md's broader
scheduler/kernel/VFS/XAM migration list starting next cycle.

## Session hygiene fixed this cycle

A stray duplicate one-shot cron job (`833b1b98`, a leftover queued-message
artifact of the recurring `1874bb39` job) was found running alongside the
real recurring `/loop` schedule and deleted. Combined with this session
having also been re-arming `ScheduleWakeup` at the end of each tick — which
is the *dynamic*-mode mechanism, not fixed-interval — the two together were
firing this loop roughly twice as often as intended. Going forward: the
recurring cron job `1874bb39` is the sole schedule; no further
`ScheduleWakeup` calls are made from within this fixed-interval loop.

No native code changed in the committed tree. New scripts (`DumpR89Sub821e60a8.java`)
were used read-only and deleted before commit, per `git status --porcelain=v1 -- scripts/`
showing empty.
