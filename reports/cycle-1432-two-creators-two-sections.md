# Cycle 1432 — two creators, two sections

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus.
- No product behaviour changed; ctest stays **53**. Contract 30 → **31
  behaviours**.
- New: `tools/ndxr_geometry_report.cpp`,
  `analysis/assets/ndxr-geometry-decode.tsv`.

## The derivation cycles 1427–1431 kept deferring

The NDXR decoder has been correct and uncontracted for five cycles, because the
section assignment — vertices at `sections.second`, indices at `sections.first`
— was a cross-match over 1227 descriptors with no retail function saying which
was which. `0x82362190` was opened at cycle 1427 and its two creators left
unread.

They are not interchangeable, and that closes it:

| section | creator | what it writes |
|---|---|---|
| `sections.first` | `0x821FBB10` | **2** at descriptor `+0`, and a 3-bit field packed from its `r5` — the call site passes 1 |
| `sections.second` | `0x821FBA78` | **1** at descriptor `+0`, and a masked address with format bits at `+28` |

So retail creates **two different resource types, one per section**, in the order
the arbitration assigns them. The one fed from `sections.second` is the one that
packs a stride-shaped field; the one fed from `sections.first` takes a 3-bit
format selector, which is the shape of an index format.

They have 41 and 10 callers respectively across the corpus.

**What is derived and what is not.** Derived: the two sections take two
different creators, and which section takes which. *Not* derived: that the type
words 1 and 2 are `D3DRTYPE_VERTEXBUFFER` and `D3DRTYPE_INDEXBUFFER` — that is a
reading of an external convention and the header says so.

The claim rests on the pair: **1227/1227 by cross-match, and a binder that makes
two distinct typed resources from exactly those two fields.**

## The entry

`retail_ndxr_geometry` is contracted with four evidences, the microexec slot
taken by the decoder's own run over every descriptor of the real package —
1227 of 1227, 44,298 restarts, per-descriptor strides, counts and bounds in
`analysis/assets/ndxr-geometry-decode.tsv`.

That artefact is not a micro-execution and the claim says so. This is a **file
format**, not an instruction sequence: there is no retail function to step that
produces a `DecodedGeometry`, and the honest evidence is the port's output over
the whole corpus with a control on every hop above it.

## On having waited

Five cycles of pictures were built on this before it was contracted, and every
one of them held. It would have been easy to contract it at cycle 1427 on the
strength of a tank that looked like a tank.

The reason to wait was never doubt that the pictures were right — it was that
"the pictures look right" and "the section assignment is retail's" are different
claims, and only the second is what a contract entry asserts. Cycle 1430's C-17
matching a real aircraft to 0.5% made the first claim overwhelming and the
second no stronger.

Two functions, thirty-six and thirty-seven instructions, settled it.

## Not established

- The element layouts inside the two vertex formats, unchanged.
- What `crash1..4` are geometrically, unchanged.
- Whether another mission's package uses vertex formats outside the two here.
  The port keeps `VertexStride`'s refusal for exactly that reason.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
ndxr-geometry decode                  1227/1227, 44298 restarts
```

## Next

Thread B's chain is contracted end to end: a mission's model byte reaches
vertices with a behaviour entry at every hop. What is drawn is still positions
and connectivity — the element layouts behind `T8`/`T18` are the next thing that
would change a picture, and they are a table read rather than a hunt.
