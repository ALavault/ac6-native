# `w224` is confirmed not a bytecode buffer: header, ~4000 bytes of padding, two small pointers, counters

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `trace_swg_w224_body`
(`frontend_state_trace.hpp`), a one-shot dump gated by the existing
`AC6_DEMO_WATCH_MODE_STATE`, `probe --until frontend --max-ticks 400`,
neutral route.

## Why

`6f87f548` found the swg "player" object's first 64 bytes (a `CSwgRenderer`
vtable pointer plus 8 sequential resource ids) and left the remaining
~4136 bytes of the 4200+-byte buffer unexamined. This closes that gap.

## The measurement

`w224+0..1024`: the `CSwgRenderer` vtable pointer, a back-pointer, the 8
resource ids already known, and **every other word through offset 1024 is
zero**. `w224+4000..4136`: also all zero, except two live pointers at
`+4116`/`+4120` (`0x2E3C4154`, `0x2E3C4294`), immediately before the
already-known frame/total counters at `+4132`/`+4136`.

Both pointers share vtable `0x82021178` — no RTTI locator at `vtable-4`, so
this class ships without RTTI (per `whose_vtable.py`'s own documented
caveat, several classes here do). Its slots overlap `CSwgRenderer`'s at
`+0x04` (`0x822CCB30`, the exact same function, 5733 calls in the neutral
route) — a shared virtual method from a common base, not a coincidence.
Several of its slots are reached, thousands of times each, from tick 106-221
onward; the hottest read slot (`sub_82311960`) is a trivial one-line getter
(`return this->field_at_4`).

## Conclusion

`w224` is a small, mostly-empty structure: a renderer-adjacent header, a
resource-id table, two pointers to small (also non-bytecode-shaped) helper
objects, and frame counters — not a compiled-clip/bytecode buffer at any
offset checked. This rules out the specific hypothesis that the "player"
object itself holds the swg movie's instruction stream. Combined with
`demo-render-chain.md`'s already-recorded finding that `sub_820E8F90` (the
actual native-call marshaller) is reached only through an unreached no-op
call, **the compiled clip data's location remains unfound** — this report
narrows where it is *not*, not where it is.

## Not established

- What the two helper objects (vtable `0x82021178`) are for; not named.
- Where the actual compiled clip/bytecode data lives, if this in-house
  format uses one at all for a screen this simple (the title screen may be
  driven mostly by native code and a small fixed command table, not a real
  interpreted timeline).

## Gates

Demo ctest 26/26 (one extraction needed: the dump was pulled into its own
`trace_swg_w224_body` function to stay under the 220-line function budget,
matching the file's established pattern for prior extractions), native gate
JF=pass, contract audits pass.
