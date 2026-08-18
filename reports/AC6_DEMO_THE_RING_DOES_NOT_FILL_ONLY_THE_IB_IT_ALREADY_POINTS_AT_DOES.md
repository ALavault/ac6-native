# Correcting `251931cf`: the ring does not fill constantly — the IB it already points at does

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `AC6_DEMO_WATCH_IB_READERS=1`
(pre-existing instrumentation, `src/guest_bridge/graphics_ring.hpp:1-36`),
`probe --until frontend --max-ticks 1000`, neutral route and a
correctly-timed `--input-at 300,16,...` (START) route, both fresh runs
against the store at `/fastdata/lavaulta/tmp/ac6-demo-ib-reader-final.MS8prn/neutral-store`.
No oracle.

## What this corrects

`251931cf` ("The ring fills constantly, the doorbell rings twice") used
`AC6_DEMO_WATCH_IB_WRITERS` — which watches writes into
`0x1274A000-0x1274CF54`, the **main indirect buffer**, not the hardware
ring — and described the result as "the ring gets 222,748 writes." That
conflated two distinct structures the report itself never named separately:

- **The ring**: `base=0x126CA000` (309108736), `capacity_dwords=131072`.
  Per the fresh `report.json`, `write_pointer=25`, `submissions=2`,
  `submitted_dwords=25` — for the **entire 1000-tick run**, in both routes.
  Two packets, 25 dwords total, both submitted at tick 0. This is not "full"
  by any measure; it received almost nothing, once.
- **The main IB**: `0x1274A000`, 3029 dwords, referenced *by* the ring's
  second packet (dwords 22-24 of the ring: header `0xC0013F00`, address
  `0x1274A000`, count `0xBD5`=3029 — this is the `region=ring_publication`
  trace line, not a separate write path). This is what receives the
  222,748 writes/1000 ticks. It is guest-mutated content sitting behind an
  already-issued "execute this buffer" command, not new ring traffic.

## The measurement

`trace_observed_main_ib_reads` (already in the tree) fires exactly when the
command processor captures/executes an IB referenced by a ring packet. Two
fresh 1000-tick runs, watching for `AC6_IB_READ`:

```
neutral route:          9 lines, tick=0 only
START-at-300 route:     9 lines, tick=0 only (identical count/shape)
```

Same 9 lines both times: 3 `ring_publication` header words (the ring's own
packet: header/address/count) + 6 `main_ib` sample offsets. Both routes:
**zero reads after tick 0.** `report.json`'s ring block confirms the same
static picture through the whole run: `read_pointer=7`, `write_pointer=25`,
`submissions=2`, unchanged from tick 0 to tick 999 regardless of the START
press.

So: the command processor captures and executes the referenced IB **once**,
at the same moment as the ring's second (and last) submission. It never
revisits that IB's content again, even though the guest keeps mutating it
222,748 times over the same window (`251931cf`).

## Which side this indicts

Two readings were possible before this measurement: (a) our own command
processor has a bug — it should re-scan an already-referenced IB when its
content changes, and doesn't; or (b) this is correct, WPTR-driven CP
behavior, and the real fault is that nothing ever posts a third ring
submission. Xenos CP is WPTR-driven, not content-driven — a real console
would stall identically if `CP_RB_WPTR` is never written again after the
first submission, regardless of what the referenced IB's memory holds. Our
model matches that: it only detects new work via ring `write_pointer`
advances, so this is (b), not a host bug to fix here.

This is also not a new finding on its own — it's the same lock `03179c5b`
and `8fba5b45` already named and exhaustively closed: the doorbell/ring-
submission path is gated on `device+21508`, itself gated on `[0x827AD2F0]`
landing in `11..19`, and nothing reachable in the codebase ever seeds it
there. What this report adds is: that static claim now has a second,
independent, dynamic confirmation — read behavior, not just write-side
gating logic — and it holds identically whether or not START is pressed at
the right time.

## Standing correction

`demo-render-chain.md`'s framing ("the ring gets 222,748 writes... real,
continuous, successful command production") described the wrong buffer.
The ring itself never grows past its first 25 dwords. What is continuous is
mutation of already-submitted-once IB content, which is real and successful
on the CPU side, but was never going to make the GPU look again regardless
— there is no re-scan mechanism on either side of this system, guest or
host, and there does not need to be one for this to be explained: nothing
ever asks for a re-scan.

## Not established

- Still not established: what would seed `[0x827AD2F0]` into `11..19` if
  `CModeTaskGameDemoOffline` were entered — this report doesn't get closer
  to that seed, it only rules out "the doorbell is starving live traffic
  that should otherwise be visible" as imprecise phrasing for the same
  underlying, already-diagnosed lock.
- Whether `indirect_buffers[0]` (address `0x127CA0C0`, 11 dwords, also
  captured once at tick 0) is meaningfully different in kind from the main
  IB — not examined here.

## Gates

No source changed; report-only commit. Demo ctest, native gate JF, and the
three contract audits all pass (nothing they check was touched).
