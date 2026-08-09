# Cycle 1377 — where the rate limits come from

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays 35. **No contract entry** — this cycle is
  research and says so.
- New artefact `analysis/flight/performance-table-lookup.tsv`.

## The answer is not a profile load

Cycle 1376 left `[model+1248/1252/1256]` — the three per-axis rate limits slot 32
uses — as "source unknown", with the expectation that they would trace to the
aircraft profile at `0x820A8678`.

They have **four writers**, and the interesting one is not a load:

| writer | what it does |
|---|---|
| `0x82282220` | the base constructor — three image defaults |
| **`0x82283480`** | **a speed-interpolated table lookup inside the object** |
| `0x822F2520` | `+1256` only |
| `0x82303D18` | slot 9 of the sibling vtable, all three |

## The defaults, and they corroborate the unit again

```
[+1248] = 5.0                   0x82069C50
[+1252] = 1.399999976158142     0x82008B0C
[+1256] = 5.400000095367432     0x82008B08
```

and immediately after them the same constructor writes **2000.0, 1800.0, 600.0,
200.0 and 150.0** into neighbouring fields. Those are the magnitudes of aircraft
speeds **in km/h** — the unit cycle 1374 established from the matched
`9.8/3.6` and `1/3.6` pair, now met a third time from an unrelated direction.

## The lookup

`sub_82283480(model, speed)`:

1. searches a **ten-entry breakpoint table** at `[model+1076 … +1112]` for the
   bracket containing `|speed|`;
2. builds the weight `1/(hi − lo)`, **guarded to 1.0 when the bracket is
   degenerate** — the same defensive shape as the atan2 guard;
3. interpolates between two **sixteen-byte records** at `[model+608 + i*16]`;
4. scales the three limits and stores them back.

The three scales are image words, resolved:

```
[+1248] *= 1/6     0.1666666716337204   0x82008BD8
[+1252] *= 1/15    0.06666667014360428  0x82007D5C
[+1256] *= 5/6     0.8333333134651184   0x820051B4
```

`1/15` is **the same word** slot 32 applies to its own row-1 axis. One constant,
used twice on the same axis at two stages.

So the aircraft's handling is a **performance curve carried inside the object**,
resampled by speed every time this runs — not a static set of numbers read once
from a profile. That is a different thing to port, and it is better to know now.

## The footprint was measured, not read

The capsule probe from cycle 1375 was reused: seed the model with a per-offset
pattern plus a monotone breakpoint table, run, diff. **Nine words change** —
`+160…+172` (one 16-byte vector), `+324`, and `+1248…+1260`.

That matters because my static scan had found only three of those stores. The
other six go through computed pointers (`r9 + r3`, `r8 + r3`), which a
displacement scan anchored on `this` cannot see. Static and dynamic disagreeing
is the useful case: the dynamic set is the true one, and the gap says the
function indexes rather than addresses.

## What is deliberately not claimed

Steps 3 and 4 above are read as a **shape**, not instruction by instruction. The
artefact says so in its own header rather than letting a confident tone imply a
derivation. The three callers — `0x82303A20`, `0x82306A38`, `0x82329968` — are
unread.

This is a research cycle with no code, and the honest accounting is below. The
plan's rule is that a gate ends with executable code; A3.2 has produced two
contracted behaviours in the last five cycles, and the next deliverable is
identified rather than deferred indefinitely.

## Not established

- The interpolation itself, instruction by instruction.
- What the sixteen-byte records at `[model+608]` hold, and who fills them.
- Whether the breakpoint table comes from the aircraft profile — that is the
  original question, now pushed one level down and better posed: **who writes
  `[model+1076]` and `[model+608]`.**

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 25 (1351–1371, 1374, 1376, 1377) |
| implementation/integration spent on A3.2 | 6 (1354–1356, 1372, 1373, 1375) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 16 behaviours
ctest                                 100% passed, 0 failed out of 35
tools/tests                           Ran 77 tests, OK
```

## Next

Who writes `[model+608]` and `[model+1076]`. Those are the aircraft's actual
performance data, and finding their writer is the link between the contracted
controller and the aircraft the player chose — the question cycle 1376 asked, now
asked one level lower where it can be answered. The same footprint probe applies:
find the writer, bound it by capsule, then read only the fields it touches.
