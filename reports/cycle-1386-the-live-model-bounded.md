# Cycle 1386 — the live model, bounded

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays 37. **No contract entry** — this cycle
  bounds, it does not port.
- New artefact `analysis/flight/live-model-slots.tsv`.

## Bounding first, again

Cycle 1384's bad news was that six of A3.2's eight implementation cycles went
into a class whose instance nothing reaches. The two functions that matter now
are the live branch's own slots 30 and 32, and the first question is not what
they do but what they cost.

| slot | function | insns | vector | calls |
|---:|---|---:|---:|---:|
| **30** | `0x82303E68` | 282 | **0** | **0** |
| 32 | `0x82306038` | 639 | 246 | 19 |

**Slot 30 is portable and differentiable by exactly the method that contracted
`0x82302DB0`** — no calls, no vector, one object through `r3`. Its footprint,
measured with a per-offset seed and a dump:

```
words written: 10 -> [304, 308, 312, 360, 364, 368, 372, 376, 1352, 1356]
static write set:    [304, 308, 312, 360, 364, 368, 372, 376, 1352, 1356]
dynamic == static:   True
```

## The port that went to the wrong class is not wasted structure

Those first eight offsets are **the same eight** `retail_flight_controls`
already models. Its reads are the same five commands at `+36…+52` and the same
flag word at `+332`, plus four inputs the other version has no use for.

So the two are **sibling implementations of one interface**, and
`ac6::retail::FlightControlState` carries over unchanged. That is the useful
consequence of cycle 1384: what went to the wrong class was the arithmetic, not
the shape.

**But the arithmetic really is different, and the constants say so before a line
is read.** `0x82303E68` loads `0.0`, `1.0` from two different addresses, `2⁻¹⁶`,
`−1.0`, `1.5`, `2/3` and `10.0`. It loads **none** of the other version's `10/3`,
`5/3`, `2.5`, `0.7`, `0.99`, `0.9`, `−0.9`, `0.8` or `0.5`. A port cannot be
adapted by renaming.

## And the shared kernel holds for the model that actually flies

Slot 32 calls `0x820A99F8`, `0x820A9B30` and `0x82211828` — the three rotations
already ported and contracted under A3.1 — and `0x8209CB70` twice, which is
`XMScalarSinCos`, certified exact at cycle 1307.

Cycle 1376 showed the shared-kernel property for `0x8200F270`'s orientation
update. It holds for the live branch too, which is the version of that finding
that was worth having.

## A second decimal literal wearing a reciprocal's clothes

Slot 32 loads `0x8200F3C4` = **0.1591549515724182**. That looks like 1/(2π) and
is not: `float32(1/(2π))` is a different word. It is the seven-digit literal
`0.15915495` — exactly the trap cycle 1374 found at `0x82008AD8`, where
`0.3183099` masquerades as 1/π.

Two of them now, in two classes. A port writing `1.0f/(2*M_PI)` would be one ulp
off on every use, and "it's obviously 1/(2π)" is again not a substitute for the
comparison. I checked this one only because the first had been caught; the
identification was written into the artefact as 1/(2π) first and corrected before
it shipped.

Both constants sit just past the live class's **40-slot** vtable
(`0x8200F310 + 160 = 0x8200F3B0`) — the constant-pool-against-the-vtable handle
of cycle 1374, now confirmed on a second class.

## Not established

- What slot 30 computes. Bounded, not read — deliberately, because a 282-
  instruction function read at the end of a cycle is how the wrong arithmetic
  gets into a port.
- Slot 39, which only the live branch has.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 8 (1354–1356, 1372, 1373, 1375, 1380, 1381) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 18 behaviours
ctest                                 100% passed, 0 failed out of 37
tools/tests                           Ran 77 tests, OK
```

## Next

Port `0x82303E68`. Everything the recipe needs is in place: the footprint is
measured and matches the static set, the constants are resolved, the interface
struct exists, and the differential is the same shape as
`audit_flight_controls_microexec.py` — which can be adapted rather than written,
since the state and input structs are the same and only the function address and
the expected arithmetic change.
