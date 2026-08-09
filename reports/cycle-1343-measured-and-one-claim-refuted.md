# Cycle 1343 — measured, and one claim refuted

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** `0x822A2030` was micro-executed five times on a
  synthetic state; no game code beyond that function ran.
- No product C++ changed, no contract changed.

## The first thing in nine cycles that could be run instead of read

Cycle 1342 refused to publish a conclusion it could only reach by reading. Five
capsules settle it, and the refusal was justified: **one of the two claims was
right and the other was wrong.**

| capsule | `[this+0x60]` low byte |
|---|---|
| baseline, fallback threshold | `0x34` |
| `f1 = +30000.0` | `0x34` |
| `f1 = −30000.0` | `0x34` |
| object filled with `32768.0` | `0x34` |
| **table threshold = −1.0** | **`0x30`** |

## Confirmed: the compared value is a constant zero

Cycle 1342 read `lfs f0, 2092(r11)` — the `0.0` at `0x8200082C` — and `fmr
f30,f0`, and concluded the comparisons run against zero. Two independent controls
say so:

**The float argument is discarded.** `+30000` and `−30000` straddle all three
thresholds and would change every bit if the argument were used. Neither moves a
bit.

**No object field feeds it either.** With the object filled with `0x47000000` =
`32768.0`, above all three thresholds, a field-fed comparison would make all
three true and set **no** bits. The word still reads `0x34`.

That second control matters because my earlier runs used a zero-filled object,
where a field reading zero is indistinguishable from a constant zero. The
distinction had to be created deliberately.

## Refuted: it is not a fixed pattern

Cycle 1342's other suspicion was that the function "sets a fixed bit pattern
regardless of anything". It does not. Setting the table threshold to `−1.0`
turns `0.0 > threshold` from false to **true**, and bit `0x04` flips off: `0x30`.

So the output is fixed with respect to the *object* and the *argument*, and
varies with the *global table*. Two very different statements, and reading could
not separate them — which is exactly why the claim was left for the instrument.

## What the function is

```
0x822A2030(this, r4 != 0):
    threshold = [[0x826E4EB4] + 0x29C80] -> +612 -> +0 -> float,  else 24000.0
    if 0.0 > threshold : clear bit 0x04   else : set bit 0x04
    if 0.0 > 12000.0   : ...              else : set bit 0x20
    if 0.0 >  6000.0   : ...              else : set bit 0x10
    -> [this+0x60], low byte only
```

A range classifier **fed a constant zero** at this call site, so it always lands
in the nearest band. Whether that is intended or a distance that arrives some
other way, this cycle does not say.

## An honest limit on the fourth capsule

The object-filled run **faulted** at step 73 — a non-zero object leads the
function into reading a pointer out of the garbage and calling address `0`. The
flag word was written *before* the fault, so the observation stands, but the run
is incomplete and nothing about what the function *returns* is established from
it.

## Not established

- Where a real distance would come from, if one does.
- What the three bits mean to their readers.
- What fills the child array; what class the children are.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

The same treatment for `0x822A1668`'s gate. It skips everything unless bit
`0x4000` of `[this+0x60]` is set, and nothing read so far sets that bit — the
reset preserves it and this classifier does not write it. One capsule over the
unit's own methods would find its writer the way this one found its inputs, and
it is a bounded question rather than another displacement scan.
