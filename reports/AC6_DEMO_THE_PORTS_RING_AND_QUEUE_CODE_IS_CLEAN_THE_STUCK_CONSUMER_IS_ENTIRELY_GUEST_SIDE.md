# The port's own ring and render-queue code is clean; the stuck consumer is
# entirely guest-side

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Read of the port's own source
(`guest_bridge/graphics_ring.hpp`, `guest_bridge/scheduler.hpp`), no probe
run, no source change.

## What this checks

`5bf9bbf7`'s corrected priority named a port-side half of `5a7c3511`'s open
circularity as the next concrete step: does this port's own
command-processor code actually consume/execute what it already receives,
or does *its own* implementation stall partway (the read/write-pointer gap
observed in an earlier run this session, `read_pointer=7` of
`write_pointer=25`)?

## Result: the Xenos MMIO ring handler is fully synchronous, no stall

`GuestBridge::apply_xenos_mmio_write` (`graphics_ring.hpp:395-610`) is the
port's entire handler for the guest's ring-kick MMIO register
(`0x7FC80714`). It is called once per guest write to that register and, in
that single call, synchronously walks every packet between the previous and
new write pointer, decodes each (`record_xenos_packet`), applies renderer-
relevant packets (`apply_xenos_typed_batch`), and -- if nothing is left
pending -- immediately calls `complete_xenos_ring_submission()`, which
writes the guest-visible read-pointer back out. There is no partial-
progress state that could "get stuck": either the whole submission
completes in the same call, or (only if `apply_xenos_typed_batch` leaves a
non-empty `xenos_pending_stream_`, e.g. a cross-tick wait) it resumes on a
later call via `resume_xenos_pending_batch()`. Neither path silently drops
or truncates work; both unhandled-packet-shape cases (`throw
RuntimeTrap("unqualified Xenos type-1/type-3 packet", ...)`) are fail-
closed, not silent skips.

The earlier `read_pointer=7`/`write_pointer=25` observation is explained
by the code, not a bug: `xenos_ring_rptr_` (the reported read pointer) is
set to `xenos_pending_endpoint_` -- a value the *guest itself* declares via
`[xenos_ring_owner_+10908]`, read fresh on every write (`graphics_ring.hpp:422`)
-- not to the raw MMIO write-pointer value. A guest-declared "endpoint" of 7
against a raw write pointer of 25 is the guest's own bookkeeping (a common
real-hardware pattern: the driver marks a fence/endpoint short of the
physical write cursor), faithfully reproduced here, not evidence this port
consumed only 7 of 25 dwords and stopped. `xenos_ring_pointer_mismatches_`
(incremented when these two values differ) is a counter, not a trap --
nothing in this code path treats a mismatch as an error.

**No bug found in this code.** The `read_pointer`/`write_pointer` gap this
session flagged as suspicious is fully accounted for by the code's own,
correctly-implemented endpoint semantics.

## The render queue `5a7c3511` measured is pure passive observation

`5a7c3511`'s "producer advances 7,495 times, consumer never moves" reading
comes from `GuestBridge::yield_guest_thread_if_due`
(`scheduler.hpp:187-220`), which -- once per scheduling quantum -- **reads
two guest-memory words** at `0x8238CD90` (producer) and `0x8238CD94`
(consumer, `kRenderQueueBase=0x82386CC0` + offsets `0x60D0`/`0x60D4`) and
counts how often each changes value. This is read-only instrumentation of
guest memory the port does not write, drive, or otherwise participate in.
**The port implements no render queue of its own here; it only watches
one the guest maintains.** A stuck consumer index is therefore
unambiguously guest-side: some guest function is supposed to read
`[0x8238CD90]`, act on pending work, and advance `[0x8238CD94]`, and
whatever that function is, it never runs (or runs and never advances the
index) on every route measured so far.

## Consequence for the plan

`5a7c3511`'s circularity is resolved on the port side: the port does not
cause the stall, at either the raw Xenos ring or the higher-level render
queue. The lock is entirely in guest code — specifically, whatever function
is meant to consume `[0x8238CD90]`/`[0x8238CD94]`. That is now the single
most concrete open question on this thread. (This exact address pair has
prior instrumentation history from before the 2026-08-17 plan reset --
`3e76ef00` "Qualify task render queue boundary" and its
`cycle-1638`..`cycle-1691` predecessors, from the old per-cycle-report era
-- but no report since the plan reset has traced its guest-side consumer;
that history should be read before extending this thread, per this
session's own repeatedly-relearned lesson.)

## Not established

- Which guest function is the intended consumer of
  `[0x8238CD90]`/`[0x8238CD94]`, or why it never runs.
- Whether `3e76ef00`/the `cycle-16xx` reports already identified this
  consumer under the old tracing convention -- not read this cycle.
- Whether this render queue is the same mechanism as the raw Xenos MMIO
  ring, a layer above it, or an independent guest-side structure that
  ultimately feeds it -- not established from the port's own code, since
  the port never writes to it.

## Process note

This report only checks code this campaign already owns (no new probe run,
no guest disassembly). Before extending toward the guest-side consumer,
`git log -S '8238CD90' --oneline --all` and a `reports/` grep were run and
found only pre-2026-08-17 (`cycle-1638`..`c90e106f`, the old numbered-cycle
era the plan explicitly retired) touching this address -- worth reading,
not worth assuming duplicates current work.
