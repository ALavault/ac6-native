# Cycle 1444 — CMapManager

## Qualification

- **No Ghidra run and no oracle pass.** The image, the corpus and
  `analysis/class-map.tsv`.
- No product C++ changed; ctest stays **53**. **No contract entry.**

## The owner

`0x82101EE8` has no callers, like `0x82303110` and `0x820A8138` before it.
`tools/whose_vtable.py` places it at **slot 2** of vtable `0x8205C9A4`, and the
campaign's own class map names it:

> **`CMapManager`**

The third named class this thread has reached by that route, after
`CX360UnitManager` (1385) and `CX360ActorModelSetup` (1422). One tool call
against an address I already had — the same move each time.

## What the class exposes

| slot | | |
|---:|---|---|
| 1 | `0x82101BF0`, 189 insns | returns a **float**; reads `y` at `+4` |
| 2 | `0x82101EE8`, 152 insns | returns **one bit** — the MCA/MCI/MCD grid |
| 3, 5, 7 | 4 insns each | a fixed vector from `this+24944`, `+24752`, `+25136` |
| 4, 6, 8 | 9 insns each | a vector from an **indexed** array, `r4*16` off `+1548`, `+1560`, `+1572` |
| 9, 10 | 2 insns each | a float from `this+24496`, `+24500` |

## Two structures, one grid

Slot 1 works from `[this+12]` and `[this+16]`. Slot 2 works from
`[this+52]`, `[this+56]`, `[this+60]` — the MCA, MCI and MCD it validates by
name.

**They share the transform exactly**: slot 1 loads the same `+65536.0` at
`0x82069BB8` and the same `1/512` at `0x82069BB4` that cycle 1442 read out of
slot 2, and applies them to `x` at `+0` and `z` at `+8` the same way.

So `CMapManager` answers **two different questions about the same 512-unit
cell grid**, from two different structures — a float from one, a bit from the
other. Slot 1 loads nothing from `+52`, `+56` or `+60`.

**The MCA/MCI/MCD grid is therefore not the height field.** That was the
likeliest guess once slot 1 turned out to return a float, and it is wrong: the
height comes from somewhere else entirely.

## What the bit is, still not established

The constraint is tighter than it was — a per-cell boolean on a map manager
whose sibling slot already answers heights, so it is not the height and not the
geometry. It could be collision, water, a no-fly boundary, or a
ground-versus-air mask.

Four cycles have now had a plausible reading available and been wrong three
times: "terrain" at 1428, "not integral" at 1440, a height stack at 1441. The
structure is complete; the name can wait for a caller.

## Not established

- `[this+12]` and `[this+16]` — the height field's own format, unread.
- The second function that references the three strings (`0x82102CCC` onward),
  which is likely the loader.
- What the three fixed vectors and three indexed arrays are.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
```

## Next

**Who calls slot 2.** The vtable is at a fixed address, so a search for
`lwz rX,8(rY)` after a load of `0x8205C9A4`, or for the constant itself, finds
the call sites — and a caller that tests the bit before moving something is
collision, one that tests it before drawing is visibility.

That is the same "follow the data flow, not the shape" move that found
`CX360ActorModelSetup` at cycle 1421 after a structural scan returned 57
candidates and nothing.
