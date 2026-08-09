# Cycle 1464 — sixty-four words

## Qualification

- **No Ghidra run and no oracle pass.** The image and the class map.
- No product C++ changed; ctest stays **56**. **No contract entry.**
- `tools/whose_vtable.py`: span 64 → 256, plus a flag.
  `INSTRUMENT_DISCIPLINE.md` gains a shape (35 → 36).

## How far the defect reaches

Cycle 1463 found `whose_vtable.py` calling slot 80 of `CMapManager` unnamed. The
question it left was whether the 64-word default had mis-answered earlier cycles.

**Two wrong measures first.** The gap to the next vtable in the class map says 34
tables exceed 64 slots and its largest is **56,260** — data sits between tables.
The run of consecutive text-range pointers says 234 and counts straight through
into the next table: `CEffectAssignTable` and `CEffectObserver` are 12 bytes
apart and both come out "433 slots".

Bounded by **both**: **10 of 811, the longest 148.** Neither proxy alone was
within an order of magnitude of that, and I published each before checking it
against the other.

**176 function addresses** sit in slots past the old default. **29 of them are
mentioned somewhere in this campaign's reports** — two are `CMapManager` slots,
`0x821002F0` at `+0x118` and `0x82102E70` at `+0x140`.

## The fix, and why it is safe

The walk **breaks at the first match**, so a longer reach can only find a start
it previously missed, never skip a nearer one. Raised to 256.

What it does risk is attributing a stray word to a distant table, so anything
resolved past the measured maximum of 148 is now flagged. Over the twelve
affected addresses the raise converts **213 unnamed hits into named ones** and
fires the flag **twice** — both on `swg::ButtonController`, the entry whose gap
measure was 56,260. The flag lands exactly where the measurement said it would.

Previously named results are unchanged: `0x820A7070` still resolves to
`CX360UnitManager slot +0x14`.

## A thing seen on the way

`0x820B0E28` appears in roughly 120 vtables, almost always at `+0x08` or `+0x34`.
I reached for it as a "deliberately stray address" to test the flag and it is
nothing of the kind — it is a shared thunk. Nothing further is claimed about it.

## Not established

- Whether any *published* finding was actually wrong because of the old default.
  29 addresses had the opportunity; cycle 1463 is the only one I can show took
  it, and it caught itself.
- What `0x820B0E28` is.

## Gates

```
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 56
instrument_discipline_index             pass shapes=36 unindexed=0
microexec_reset_completeness            pass fields=27 constants=6 missing=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Back to `+0x30`.** The instrument detour is finished and the map question is
where cycle 1463 left it: the draw path is slot 80, it is live, and it abandons
on a field no reachable writer sets. `0x82105738` and `0x820943B0` are the two
functions that receive a map pointer and pass it on, and neither has been read.
