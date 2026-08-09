# Cycle 1378 — the two flight models differ in kind

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays 35. **No contract entry** — research.
- New artefact `analysis/flight/performance-table-owner.tsv`.

## Both naive searches were useless, and one of them was a trap

Cycle 1377's "next" was: who writes `[model+608]` and `[model+1076]`.

```
stores into [+608..+767] or [+1076..+1112]   58 functions
`addi rX,rY,608` or `,1076`                  104 sites, nearly all stack frames
```

608 is a common frame size *and* a common field offset. Filtering to methods of
this class family — the 36 slots of `0x82008B10` / `0x8200F270` / `0x8200F310` —
leaves **exactly one**.

And the filter did not merely narrow; it **refuted a wrong answer**.
`sub_8225CB20` stores to `+1076`, `+1080`, `+1088`, `+1092`, `+1096` and `+1104`
— a near-perfect match for a ten-entry breakpoint table. It is not in this family
and is not called by it. It is a different class's field at the same
displacement, and without the filter it would have been the answer.

That is the fifteenth displacement collision this campaign, and the first where
the false candidate matched the *shape* of the table and not just the offset.

## The one writer

`sub_82282408` — **slot 9**, on the base vtable and inherited unchanged by
`0x8200F270`. 311 instructions, 40 stores into `+608…+767`. The sibling
`0x8200F310` overrides slot 9 with `0x82303D18`, which chains to the base and
then writes the three rate limits itself.

## The asymmetry, and it is the finding

| slot | base | `0x8200F270` | `0x8200F310` |
|---:|---|---|---|
| 9 | `0x82282408` | `0x82282408` (inherited) | `0x82303D18` |
| 29 | `0x822DDBE8` | `0x822DDBE8` | `0x82303A20` |

`0x822DDBE8` is the **shared empty virtual** — a single `blr`, identified at
cycle 1371.

Slot 29 is where the speed-interpolated lookup `sub_82283480` is driven from.
**`0x8200F270` leaves it empty.** That is the class that owns the position
integrator `0x82303110` and the control surfaces `0x82302DB0` — both contracted.
Only the sibling, whose own slot 31 is 505 instructions of vector aerodynamics,
resamples the performance table.

So the two flight models inside one entity **differ in kind**, not only in code:

- `0x8200F270` takes its rate limits from whatever slot 9 established and keeps
  them;
- `0x8200F310` re-derives them from a performance curve whenever slot 29 runs.

This matters for the port directly. Cycle 1377 wrote that the handling is "a
performance curve resampled by speed, not a static set of numbers" — that is true
of the **sibling**, and the class the campaign has contracted does the opposite.
The correction is to that cycle, one day old, and it is exactly the kind of
statement that would have been left standing because it sounds more sophisticated
than the truth.

## What the capsule could and could not do

A probe on `0x82282408` with a flat per-offset seed **faults after 79 of 311
steps**, having written 53 words in seven runs — `+16…+35`, `+128…+139`,
`+144…+255`, `+320…+351`, `+356…+383`, `+420…+423`, `+1296…+1299` — and **none**
of them in the record table.

It reads pointers, so a flat pattern is not an environment it can run in. That is
reported rather than worked around: the 53 words are real (both poison passes
agree), the record-table stores are on a path this run never reached, and where
the data comes from is open.

## Not established

- What `0x82282408` copies from, and therefore where the aircraft's performance
  data enters the object.
- Whether the player's aircraft is a `0x8200F270` or a `0x8200F310`. This is now
  a sharp question with a testable consequence, where before it was a naming
  question.
- `0x82306A38` and `0x82329968`, the lookup's other two callers.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 26 (1351–1371, 1374, 1376, 1377, 1378) |
| implementation/integration spent on A3.2 | 6 (1354–1356, 1372, 1373, 1375) |

Four research cycles since the last code. The next slice should end with a
behaviour, and the candidate is named below.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 16 behaviours
ctest                                 100% passed, 0 failed out of 35
tools/tests                           Ran 77 tests, OK
```

## Next

**Which class the player's aircraft is.** The entity constructor `sub_8222BEC8`
builds a `0x8200F270` at `+2224` and a `0x8200F310` at `+3536`, and initialises
`+4912` — the pointer every other subsystem reads — to the **`0x8200F310`** one.
The derived entity `sub_82293C28` repoints it at a third object.

If `+4912` is the model that flies, the campaign has contracted the integrator
and control surfaces of the class that is *not* pointed at, and that has to be
established before more is built on it. It is a bounded question: find where
`+4912` is written after construction, which cycle 1371 already enumerated as two
sites.
