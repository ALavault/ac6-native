# Cycle 1450 — the index is unbiased

## Qualification

- **No Ghidra run and no oracle pass.** The flat image via `tools/ppc_read.py`,
  and the archive.
- Product C++ **added**: `retail_map_water.{h,cpp}` and its test.
  ctest **55 → 56**.
- Contract: `mission01-playable-gate-v1.json` gains `retail_map_water`,
  **33 → 34 behaviours**.

## What the port found

Reading `0x82101EE8` instruction by instruction to write the derivation turned up
something three cycles of tooling had wrong.

`f11` and `f10` are loaded once — `lfs f11,0(r4)` and `lfs f10,8(r4)` at
`0x82101EF4`/`0x82101EF8` — and **never written again**. The `+65536.0` at
`0x82101F14` lands in `f13`/`f12`, which become the cell. Then at `0x821020EC`,
four hundred bytes later, retail multiplies the *untouched* `f10`/`f11` by the
`0.125` at `0x8200322C`.

> **The bit index uses the raw coordinates. The world bias never reaches it.**

And `fctiwz` truncates toward **zero**, so this is not the same as biasing and
flooring: for `x = -100` retail indexes bit 52 and the biased form indexes 51.
Cycles 1445, 1447 and 1449 all used the biased form in tools.

## Measured, not assumed, because the consequence is smaller than the defect

```
128-unit lattice, 262,144 samples : 0 bits differ (0.000%)
off-lattice, 200,000 probes       : 43 bits differ (0.02%)
agreement with land               : 97.43% either way
```

The **index** differs for every negative non-integral coordinate. The **bit**
almost never does, because adjacent 8-unit cells agree except at a boundary. And
on the lattice those cycles actually sampled it is provably identical: the world
coordinate is `s * 128 − 65536`, so `world / 8` is the exact integer `s * 16 −
8192`, and 8192 is a multiple of 64.

So **cycle 1445's 97.43% stands unchanged** and nothing published moves. I nearly
wrote "every water lookup in the negative half was one cell off" into the header,
which is true of the index and false of the consequence; the number is in the
header instead.

## The port

The chain, every step at an address, is in `retail_map_water.h`. Two things it
does that a convenience decoder would not:

- **the magics are compared byte by byte**, starting at `0x82101FC8`, because
  that is what retail does and it is why two searches for them as 32-bit
  constants returned zero (cycle 1442);
- `query` returns **false** wherever `0x82101EE8` returns −1 — outside the coarse
  grid, or a bounds check refused — rather than inventing a bit. The bounds are
  the u16 at `MCI+8` (4,864) and at `MCD+8` (413), read as retail reads them.

## The measurement is re-run, not cited

The test reproduces cycle 1445's number rather than quoting it: 97.43% over
262,144 samples, with the residual required to be 90%+ flat ground with the bit
clear. And a control the earlier cycle did not have:

```
the open sea    20480/20480 set     (100%)
the inland      19082/19200 clear   (99.4%)
```

A decoder whose index were shifted would blur both, so those two numbers are the
check on the fix that found the defect.

## And the renderer stops carrying its own copy

`tools/mission01_city_render.cpp` had an inline water lookup, written in cycle
1449 because there was no port. It now calls `MapWaterGrid`. That is the point of
porting: the picture and the contract read the same bytes through the same code.

## Not established

- What the game *uses* the bit for. `is_water` is named from a measurement and
  the header says so, with a refutation condition. `0x82101EE8` still has no
  caller in the corpus.
- `.mia`/`.mid`/`.mta`/`.mti`/`.edl`.
- `tag >> 16` in the placement list.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
contract_addresses                    pass cited=317 supported=317
contract_derivations                  pass behaviours=52 gaps=0 multiple=0
tools/tests                           Ran 79 tests, OK
```

The gate refused this cycle's contract entry twice before accepting it, both
times for a `retail_addresses` entry the derivation did not literally cite. Both
were fixed by citing the address in the header rather than by dropping it from
the claim.

## Next

**`tag >> 16`, with the control that now exists.** Every building is drawn
axis-aligned and the street grids do not line up. If the field is a rotation,
applying it makes them line up, and that is visible and falsifiable in one
render — which is exactly the kind of test cycles 1428, 1440 and 1441 lacked when
they guessed.
