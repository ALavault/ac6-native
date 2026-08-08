# Cycle 1210 — the far-side rule, applied to my own numbers within the hour

`INSTRUMENT_DISCIPLINE.md` gained a rule this cycle: **before publishing a join,
query it from the far side.** Here it is run against this session's own texture
join, and it changes two numbers and kills a third.

## The join, over both corpora instead of one

Every figure this session came from the **537 standalone `.ndxr`** files. The
`001_MDLP.mdlp` holds 292 more. Walking both with the same reader:

| | standalone only | union |
|---|---|---|
| NDXR texture references | 179 | **616** |
| GIDX ids available | 205 | **663** |
| references resolved | 179 / 179 = 100% | **539 / 616 = 87.5%** |
| textures never referenced | 25 of 205 | **124 of 663** |

**A hundred-percent join became 87.5%, and "25 unreferenced" became 124.** A
number that moves five-fold when the other corpus is added was never a property
of the format; it was a property of the extraction. Cycle 1209 corrected an
*interpretation*; this corrects the *coverage figures* every cycle since 1207 has
been quoting.

The 77 unresolved references are **all** high-nibble 1 — `0x10000C67` through
`0x100026A7`, not one below `0x10000000`. An independent walk reported 77 distinct
unresolved ids for its own corpus; mine, from a different traversal, gives the
same 77. Two walks agreeing on a number neither was tuned to produce.

The 124 unreferenced split 22 small and 102 large.

## A near-miss refused

Cycle 1207 recorded **102** MATE materials matching no NDXR-embedded material.
This cycle finds **102** unreferenced textures of the large-id form. The
temptation is obvious and the answer is no: those 102 materials reference **195**
ids, not 102, so the two counts are of different things — materials on one side,
textures on the other — and coincide only in cardinality. This is the shape cycle
1198's `0x118` quotient had and cycle 1208 fell for. Recorded and not joined.

## The far side that could not be queried, and the control that proved it

The obvious next step: the 77 name textures in packs absent from the extraction —
so look in the retail archives. Scanning all 2.93 GB of `DATA00.PAC` and
`DATA01.PAC` for the `GIDX` tag returned **zero**.

That zero is worthless, and the control says so:

| tag | `DATA00.PAC` (2.27 GB) | `DATA01.PAC` (0.66 GB) |
|---|---|---|
| `GIDX` | 0 | 0 |
| `NDXR` | 1 | 0 |
| `NTXR` | 1 | 0 |
| `MATE` | 1 | 0 |
| `FHM ` | 0 | 1 |
| `NUP3` | 0 | 0 |

A specific four-byte string occurs by chance in 2.27 GB of random data
`2.27e9 / 2^32 ≈ 0.53` times. **Every count above is at the chance rate**, and
both headers are high-entropy (`69 de 05 8b 00 4e 50 d8 …`). The containers are
compressed or encrypted, which is precisely why the cycle-738/739 extraction
pipeline exists.

So the scan measured nothing about the 77. **The far side is real but not
reachable by this instrument**, which by the rule just written belongs in *not
established* rather than in the conclusion — and would have read as "those
textures exist nowhere" had the control not been run.

The rule was written an hour ago and has now caught its author twice: once in the
coverage figures above, once in this zero.

## Not established, stated plainly

- Whether the 77 references resolve inside the PAC archives. It needs the
  extraction pipeline, not a byte scan, and was not run.
- What the 124 unreferenced textures are for. Twenty-two carry small ids, which
  the `mapparts` corpus otherwise monopolises.
- Cycle 1207's 195 MATE-only ids, still untouched.
- The vertex data, still the blocker: no field on the derived path addresses it.

## What this does not change

The material → texture → NTXR chain itself. Its derivation
(`0x82355318` → `0x8233EE40` → registry `0x828C8100` ← `0x8234BEC8` ←
`0x82340870`, keyed by `GIDX+0x08`) and its six-rival discriminating control
stand untouched. What moved is how much of Mission 01 the available bytes let
that chain reach: **87.5%, not 100%.**

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
616 references, 663 GIDX ids, 539 joined; 2.93 GB of PAC scanned with a tag control
```

No product code changed. No retail bytes committed.
