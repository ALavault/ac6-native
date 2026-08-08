# Cycle 1166 — the pack closes: a header texture, N−2 records, and a terminator

## Using the derived section base

Cycle 1165 derived section 1's base from `0x8234B28C` — `descriptor + [halfword
at +0x0C]` — instead of the hard-coded `0x60` cycles 1163/1164 had used. Re-run
with it:

```
record slots resolved      522
bad eXt/GIDX signatures      0      (was 4)
skipped                      0
```

The four packs cycle 1164 called anomalous are not. Their section base is `0x90`,
and at `base−0x20` sits `eXt\0` and at `base−0x10` sits `GIDX`, exactly as
everywhere else. They were four files this port was reading with a hard-coded
base, precisely as cycle 1165 predicted.

## What the 86 refusals were

The decoder refused 86 of the 522 slots, all with `PayloadSizeMismatch`. They are
**null records**: width 0, height 0, level count 0.

There is exactly one per pack, and it is always the **last** slot:

```
packs with a null slot          86 / 86
null slot index                 always count − 1
```

So the structure closes:

| | |
|---|---|
| header word 1, low half | the texture count **N** |
| the file's own descriptor | texture **0** |
| records at `section1 + 0x50·i`, `i` in `0 … N−2` | textures **1 … N−1** |
| slot `N−1` | a terminator |

The arithmetic checks on both ends. A pack declaring 5 yields 4 real records plus
the header texture — five. And the four packs declaring **1** have zero records
and their single texture in the file header, which is exactly why they were the
only four that decoded standalone back in cycle 1162, before any of this was
understood.

## Every real record decodes

```
record slots      522
terminators        86
real records      436
decoded           436
```

All of them, with the product decoder unmodified. The 86 header textures are a
separate population and are not decoded here: a pack's file-level payload spans
every texture it holds, so extracting texture 0 needs its own slice, which is the
same diagnostic step already applied to the records.

## What is derived and what is not

**Derived**: section 1's base (`0x8234B28C`), section 2's base and therefore the
data offset (`0x8234B284`), the descriptor field layout (`0x8234B360`,
`0x8234B128`, `0x8234B118`), the surface arithmetic (`0x821DF838`, `0x821DF958`
under `XGSetTextureHeader`), and the chunk layout as retail's own writer emits it
(`0x821D9478`).

**Still measured**: the `0x50` spacing between consecutive records, and the
terminator convention. `0x8234B268` locates a section and then walks mip levels
inside it; nothing read so far walks sibling records, so the thing that would
derive both — the sibling walker — has not been found. 522 slots resolving with
zero bad signatures and exactly one terminator per pack is strong evidence about
a layout and is not a reading of the code that produces it.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed. Decoded pixels stay local.
