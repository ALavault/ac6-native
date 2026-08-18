# The swg per-element draw call runs successfully, five hops deep, for the whole run

## Qualification

AC6 demo PAL, same XEX SHA-256 as the rest of this chain. Static evidence:
`codegen/generated/ppc_recomp.{0,6,22,36,37,39}.cpp`. Reachability control:
the valid 12000-tick neutral atlas (`analysis`-adjacent scratch,
2711 functions, same one used and cross-checked in `a1d0106e`).

## Where this comes from

`6f87f548` found the swg "player" object holds 8 allocator-assigned resource
IDs at `+16..+44`, not bytecode, and named `sub_820EB200`
(`CSwgRenderer` vtable slot `+0x1C`) as the hottest slot (165429 calls over
12000 ticks) without reading it. This reads it.

## What `sub_820EB200` does

Given an index `i` (from `this+12`), it reads `self[(i+4)*4]` — one of the
8-entry table `6f87f548` found. If that slot is `-1` (empty), it returns
immediately. Otherwise it builds a full draw parameter block on the stack
(position, UV, color, flags derived from per-element byte flags at
`self+4112`/`+4113`), then calls `sub_820EA9A0` (a small copy/math helper)
and `sub_82095DF0`.

## The chain, traced hop by hop, with reachability at each hop

```
sub_820EB200   165429 calls, tick 266-11999   (per-element draw, "-1" check
                                                never taken — every call has
                                                a valid resource id)
  -> sub_82095DF0   341527 calls
       -> sub_821DEED8   341527 calls
            -> sub_822DB930   341527 calls
                 -> sub_822E68F8   511742 calls
                      -> sub_822F3270   511742 calls
                           -> __imp__RtlEnterCriticalSection
                           -> sub_822F5BC8 (pool allocator, indexes a
                              288-byte-element array)
                           -> __imp__RtlLeaveCriticalSection
```

Every function in this chain is reached unconditionally alongside its
caller (the counts either match exactly or the ratio is consistent with fan-
out from other, unrelated callers joining the same shared helper — expected
for generic engine plumbing). `sub_820EB200`'s own `-1` early-exit is never
taken in this run: every one of its 165429 calls carries a live resource id
and proceeds to build and submit a draw.

## Where the chain goes cold

`sub_822F3270` is a critical-section-guarded call into `sub_822F5BC8`, which
indexes a 288-byte-element array — generic pool allocation, not obviously
D3D-specific. The count growth at this hop (165429 → 341527 → 511742, not a
flat 1:1 ratio) confirms this is **shared plumbing used by more than just
the swg draw path**, not a private pipe. This report does not establish
whether draws built here ever reach the D3D command ring
(`03179c5b`/`8fba5b45`'s subject) — that connection was hypothesized while
tracing this chain and is explicitly **not shown** by it. Confirming or
refuting it needs either reading `sub_822F5BC8` itself or a live memory
correlation, neither done here.

## What this does establish

The framing "nothing is rendering" undersells the CPU side: the swg title
screen's per-element draw logic runs successfully, with valid resource ids,
continuously, for the entire 12000-tick run, at least five function calls
deep, without hitting an obvious dead end or an unreached function. Whatever
makes the screen stay black is not "the renderer never tries" — it is either
further downstream of this chain, or a content problem (the resources behind
those 8 ids resolving to something that renders as black), or both.

## Not established

- Whether this chain terminates in the D3D ring, in an off-screen surface
  that's never presented, or somewhere else — the trail goes cold at a
  generic critical-section-protected allocator.
- What the 8 resource ids resolve to, and whether they are valid loaded
  textures or empty placeholders.
- Whether this chain is representative of the title screen's *visible*
  elements or a background/off-screen pass.

## Gates

No source changed this cycle; report-only commit.
