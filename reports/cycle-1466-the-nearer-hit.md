# Cycle 1466 — the nearer hit

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py`.
- No product C++ changed; ctest stays **56**. **No contract entry.**

## What slot 80 is

`0x82102E70` is 333 instructions and calls both of its children with the **same
two points**:

```
0x82103010  bl 0x82102148          the placed-parts branch
0x82103014  r11 = r3 & 0xFF
0x8210301C  if zero -> skip
0x82103024  li r18,1               a hit flag
0x82103040  stw r11,0x0(r23)       its out-parameter, to the caller

0x82103068  bl 0x82102568          the terrain branch
0x8210306C  r11 = r3 & 0xFF
0x82103074  if zero -> skip

0x8210308C  lfs   f0,0xC(r25)      the best distance so far
0x82103090  lfs   f13,0x8C(r1)     the terrain hit's distance
0x82103094  fcmpu f13,f0
0x82103098  bc 4,24 -> skip        keep the parts hit unless terrain is nearer
0x821030B8  stw r11,0x0(r23)       otherwise overwrite with terrain

0x821030BC  if no hit at all -> return
0x821030CC  and -1 in the out-parameter is also "no hit"
```

> **`CMapManager` slot 80 is a segment query against the map, returning the
> nearest hit among the placed parts and the terrain.**

## Which dissolves four cycles of worry

Cycles 1462–1465 chased the writer of `+0x30` because `0x82102148` abandons when
it is zero, and a gate nothing satisfies looked like a broken system or dead
code. It is neither. A zero `+0x30` means **no placed parts to test**, the
function returns zero, and `0x82102E70` goes straight on to the terrain branch
and answers from that.

The empty case is a normal case. Four cycles of bounded negatives were searching
for a writer that a map without part-collision data does not need.

## And a name I got wrong for eleven cycles

I have called `0x82102148` "the `.pdl` **draw** path" since cycle 1454, and cycle
1455 called `tag & 0xFFFF` "a **draw** argument" passed to `0x822C2868`.

`0x82102E70` compares distances and keeps the nearer. Nothing here draws.
`0x822C2868` — 566 instructions, VMX-heavy, taking a `.nud` resource, a
stack-built transform and the two endpoints — is a **segment-versus-mesh
intersection**, and "draw" was an inference I never marked as one.

What survives untouched is everything read out of instructions: the `.pdl`
header stride, the `count`/`offset` partition, the `coarse * 8192 - 61440`
transform, the tag's four fields. A layout does not change because the function
around it turns out to answer a different question. What changes is the word
"draw" in three reports, and the contract statement, which says *consumes* and
not *draws* — that one was written carefully and does not need amending.

## Not established

- Whether `0x822C2868` is an intersection. It is the reading the caller now
  favours, and 566 VMX-heavy instructions have not been read.
- The writer of `+0x30`, which is no longer interesting: a map with no
  part-collision volumes has nothing to write there.
- `[r25+0xC]`, the running best distance, and who owns `r25`.

## Gates

```
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 56
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Port the segment query, or leave the map and go back to the aircraft.** The
query is now understood end to end at the level the product would need — two
branches, nearest hit, `-1` for none — and it is the piece that puts an aircraft
in contact with the world it flies over. The alternative is that Thread A has had
no cycle since 1417 and the flight model is where "playable" is actually decided.

Twenty cycles of Thread B have produced terrain, water, placement and a city
under contract. That is a good place to choose deliberately rather than by
momentum.
