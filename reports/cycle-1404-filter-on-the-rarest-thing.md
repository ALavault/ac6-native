# Cycle 1404 — filter on the rarest thing you know

## Qualification

- **No Ghidra run and no oracle pass.** The corpus.
- No product C++ changed; ctest stays 47. **No contract entry.**
- `analysis/flight/command-caller-search.tsv` extended.

## The bet was half right

Cycle 1403 stopped the search and bet on "a second writer of `+36`/`+40`/`+44`
**outside the class family**". Writers of all three: **142 functions**.

Useless, and for the fifth time in this search the same reason — `+36`, `+40`,
`+44` are three consecutive floats, so every vec3 setter in the image matches.

## The filter that worked was the other half of the same sentence

Cycle 1403 also said the filter should be "on *data*, not on the call graph".
Cycle 1352 established that the input consumer `0x82211DF8` hands `0x82211C10`
**four output arrays** at `this+0xE58`, `+0xED8`, `+0xF58`, `+0xFD8` — 3672,
3800, 3928, 4056.

**Two functions in the entire corpus touch any of those four displacements.**

```
sub_820CC918   243 insns
sub_82229250   367 insns
```

Two, out of roughly eight thousand. That is not a collision; it is a field
nobody else uses.

## And one of them is on the input path from both sides

`sub_82229250`:

- **materialises `0x826EDB98`**, the contracted input record array — one of the
  nine sites cycle 1403 enumerated;
- **touches the binding layer's output arrays**;
- its **only caller is `0x82234040`** — the entity per-frame function that cycle
  1396 showed reads `entity+4912`, the live flight model pointer, **seventeen
  times**, more than any other function in the image;
- it **calls `0x82228480`**, which cycle 1371's scan recorded reading
  `entity+4912` and dispatching a slot on it.

```
0x82234040   the entity's frame
  -> 0x82229250   reads the input records AND the binding outputs
    -> 0x82228480 reads entity+4912 and dispatches into the flight model
```

## What it is not, yet

**This is not the setter call.** `0x82229250` writes none of the three command
fields, and its eight virtual dispatches are all at offset 32, which is generic.

What it *is*, is the first function established to sit on **both sides** of the
boundary the demo's invented link spans — and it was found by filtering on a
field that two functions use rather than on a slot that 151 share.

## The lesson, and it is the one this search cost six cycles to learn

**Filter on the rarest thing you know.**

| handle | functions sharing it |
|---|---:|
| a virtual slot number | 78–151 |
| a vec3 displacement | 142 |
| **the binding layer's output array** | **2** |

Every filter this search tried before was built from something the binary reuses
everywhere. The one that worked was built from a field the contracted work had
already established as unique. Six cycles of refutations were the cost of
reaching for slot numbers first because they were the thing in front of me.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 8 (1395, 1396, 1399–1404) |
| implementation/integration spent on A3.3 | 2 (1397, 1398) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 47
tools/tests                           Ran 77 tests, OK
```

## Next

Read `0x82229250` — 367 instructions, no vector, six calls, and it is the only
function known to see both the contracted input and the live flight model. If the
stick-to-command conversion exists anywhere as a single readable rule, it is
here or in `0x82228480` one hop further.
