# The ring fills constantly; the doorbell rings twice — the full picture

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `AC6_DEMO_WATCH_IB_WRITERS=1`
(pre-existing instrumentation, `src/guest_memory.cpp:19-39`), `probe --until
frontend --max-ticks 1000`, neutral route, no oracle.

## What this closes

`6fbe43f6` traced the swg per-element draw call five hops deep and
explicitly could not show whether it reaches the D3D ring — the trail went
cold at a generic pool allocator. Rather than trace further by hand, this
uses instrumentation that was already in the tree and answers the question
directly, without needing to identify the exact call chain.

## The measurement

```
rc=4 (max_ticks reached cleanly)
222748 AC6_IB_HOST_WRITE lines in 1000 ticks
```

Every one of the top 20 most frequent write addresses (28-47 hits each)
resolves, via `addr2line` on the module offset every line reports, to:

```
(anonymous namespace)::guest_memory_access<AC6_PPC_STORE_U32::{lambda}>
  (guest_bridge.cpp:396)
```

— the generic host-side implementation every guest `PPC_STORE_U32` call
expands through. This is the same store shape `c2820200` already
established for the ring-append function (`sub_821BA1F8`,
`PPC_STORE_U32(ea, ctx.r6.u32)`), consistent with ordinary guest `stw`
instructions, not some other write path.

## The full picture

```
guest code (swg draws, and whatever else)
  -> writes real command dwords into the indirect buffer, continuously
     (222,748 times per 1000 ticks — this is not idle, it is heavy,
     sustained traffic)
  -> sub_821BAA78 reserves ring space, sub_821BA1F8 appends it
     (03179c5b, c2820200 — both reached every tick)
  -> the doorbell, sub_821B9BC8, only ever writes the GPU's wptr MMIO
     register (0x7FC80714) twice, both at tick 0
     (03179c5b: gated on device+21508, itself gated on [0x827AD2F0]
     landing in 11..19, which nothing reachable ever sets — 8fba5b45)
  -> the GPU is never told there is anything new to consume, ever again
  -> the screen stays black despite continuous, successful CPU-side
     command production
```

This resolves `6fbe43f6`'s open question in substance, if not in exact call
chain: whatever writes 222,748 times into the IB per 1000 ticks, it is
consistent with — and there is no evidence against — the swg draw chain
(and whatever else runs alongside it) being a real contributor. The block
was never "nothing builds commands." It is, precisely and only, "nothing
tells the GPU to read them after the first frame."

## Standing correction

`demo-render-chain.md`'s framing of `03179c5b`/`8fba5b45` as "a separate,
narrower thread... not on this critical path" undersold it. The mission-
scoped render gate (`device+0x5460`, `AC6_DEMO_RENDER_GATE_RAISER.md`) is a
real, independent, also-unarmed gate for a *different* function
(`sub_821C57D0`) — that finding stands. But the doorbell
(`sub_821B9BC8`/`[0x827AD2F0]`) is not narrower than the black-frame
question; it is one of (at least) two independent locks on rendering, and
the one this report shows is actively starving real, continuously-produced
command traffic.

## Not established

- The exact guest PC(s) performing these writes — `dladdr` resolves the
  shared host template, not the per-call-site generated function; getting
  that needs either a different instrumentation point or reading the
  `x.trace` file's own PC log, not done here.
- Whether fixing the doorbell alone (seeding `[0x827AD2F0]` into `11..19`
  from somewhere) is sufficient, or whether the mission-scoped gate
  (`device+0x5460`) also needs arming for the *specific* draws that matter
  for the title screen — these may or may not be the same submissions.

## Gates

No source changed; report-only commit.
