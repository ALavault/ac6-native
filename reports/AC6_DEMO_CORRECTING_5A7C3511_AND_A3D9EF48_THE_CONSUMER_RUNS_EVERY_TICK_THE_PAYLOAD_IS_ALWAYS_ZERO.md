# Correcting `5a7c3511` and `a3d9ef48`: the render queue's consumer runs
# every tick; the actual defect is that its payload slots are always zero

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Read of seven pre-2026-08-17 reports
(`cycle-1638`..`cycle-1777`, the numbered-cycle era `c90e106f` retired) plus
their JSON receipts. No new probe run, no source change.

## What was wrong

`a3d9ef48` (this session) named "which guest function is meant to consume
`[0x8238CD90]`/`[0x8238CD94]`, and why does it never run" as the open
question, taking at face value `5a7c3511`'s (2026-08-18, current lineage)
measurement: "the render queue's producer advances 7,495 times over 4,000
ticks. The consumer does not move once." Both the question and the premise
behind it are already answered, by five independent report generations that
predate the current investigation's own awareness of them.

## What was already established, before the plan reset

`cycle-1639`/`cycle-1642`/`cycle-1643`/`cycle-1690`/`cycle-1691`/`cycle-1777`
(five separate live-probe reports, consistent with each other): the
consumer function is **`sub_820FFCA0`** (write site `0x820FFD78`, with a
reset-both-indices-to-0 pair at `0x820FFD94`/`0x820FFD98`), running on a
dedicated worker thread (thread 25). Measured reached **every single tick**
across every window tested -- 48/48 ticks, later 348/348 ticks, across both
the neutral route and a (mistimed, tick-252) `buttons16` route. The
producer is `sub_820FF710` (write site `0x820FF734`, thread 1).

**`cycle-1642` explains exactly why the scheduler's aggregate counter
reports zero consumer activity despite this**: that counter
(`render_queue_consumer_changes`, the same field `5a7c3511` cites) samples
the queue's two index words once per scheduling quantum. The worker resets
both indices back to 0 *within the same tick* it consumes, before the next
sample. So the coarse counter structurally cannot see the change -- it
observes a 0-to-0 transition every time, even though the guest's own exact
memory-store trace shows real consumption happening every tick. Quoted
directly: *"ce compteur observe le ring après les resets du worker et
annonçait consumer_changes=0, alors que les stores guest exacts montrent
une consommation à chaque tick."* This is a measurement artifact of the
scheduler's own quantum-sampled instrument, not a property of the guest.
**`5a7c3511`'s "the consumer does not move once" was a re-discovery of this
exact artifact, five report-generations after it was already explained.**

## What `a3d9ef48` gets to keep

The narrower, code-level claim stands: the port implements no queue logic
at `0x8238CD90`/`0x8238CD94` itself, only passive observation
(`yield_guest_thread_if_due` in `scheduler.hpp`), and the Xenos MMIO ring
handler (`apply_xenos_mmio_write`) is genuinely fully synchronous with no
stuck partial-progress state. Both of those are about the port's own code
and are unaffected by this correction. What falls is the inference drawn
from `5a7c3511`'s guest-side numbers: there is no stuck consumer to explain.

## What the old reports show is the actual, still-open defect

The index dance (producer/consumer counters) runs correctly, every tick,
proven. What is *not* proven is that anything real ever gets queued.
**The payload slots -- `0x82386DD0` (producer's slot) and `0x82386D90`
(consumer's slot), roughly `0x100` past the index pair -- are always
all-zero**, confirmed by SHA-256 across multiple fresh windows and both
routes in `cycle-1690`/`cycle-1691`/`cycle-1777` (matching hashes each
time, no run-to-run variation). `cycle-1777`'s own summary states it
plainly: `"queue_producer_consumer_proven": true,
"task_menu_owner_constructed": false"`. Corroborating, independent
evidence from the same report: the mode manager's own task dispatcher
(`0x82259D10`, slot 4, callsite `0x82259D74`) carries exactly three stable
entries the whole run -- `CModeTaskStartUpDemoOffline`, `CTaskLoading`,
`CTaskModeManager` -- on both routes, byte-identical. No menu or mission
task is ever added to the list this dispatcher would run.

`cycle-1643` named, but never chased, the consumer's own call site,
**`0x820FEFA8`** -- what it does with the (always-empty) payload it reads
is unestablished.

## Consequence for the plan

The render-queue mechanism is fully plumbed and running correctly every
tick on both CPU sides (producer and consumer threads both alive, both
active). It is starved, not stuck: nothing in the currently-reachable guest
code ever writes real content into the slots it reads from. This reframes
the open question precisely: not "why doesn't the consumer run" (it does)
but **"what should be producing real payload into
`0x82386DD0`/`0x82386D90`, and why doesn't it" / "what does `0x820FEFA8` do
with an empty payload, and does its own logic explain why the task
dispatcher's list stays at exactly three entries."** This connects cleanly
to everything else this campaign has independently found: the task list
never grows past its three startup entries, no menu owner is ever
constructed, and the frontend is a working attract loop with nothing behind
its curtain.

## Not established

- What `0x820FEFA8` does, or what would need to write non-zero payload for
  it to act.
- Whether this queue is upstream, downstream, or parallel to the Xenos MMIO
  ring's own stall (`c2820200`) -- not connected in either direction by any
  report read so far.
- Whether a correctly-timed START press (`651e7878`'s tuple, not the
  mistimed tick-252 `buttons16` route the old reports used) changes the
  payload-slot or task-dispatcher picture -- the old reports predate that
  timing correction; not re-measured this cycle.

## Process note

This is the second time this session a current-lineage report (`5a7c3511`,
2026-08-18) re-discovered a symptom an older report generation had already
explained, without citing it, because the older work used the
now-retired numbered-cycle convention and a keyword/address search alone
does not surface a correction of a *counter's own measurement validity* --
only a correction of what the counter's *value* means, which is exactly the
gap [[forward-chain-checks-need-content-search]] already names but had not
yet seen in this specific form (a re-discovered artifact rather than a
re-discovered address). Read the old numbered-cycle reports for any address
this campaign is about to build a conclusion on, not just the current
lineage's own commits.
