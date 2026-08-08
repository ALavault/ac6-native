# Cycle 1242 — why the UV control works, and why a position control would not

## The attempt

Cycle 1241 named the geometry path as having neither of the suite's two control
styles. The obvious repair is the technique that already paid: a plausibility
control on POSITION, with a shifted read as the named rival, exactly as the UV
control does.

Measured first, over 265,458 stride-32 vertices:

| POSITION read at | finite and `|v| <= 1e6` |
|---|---|
| `+0` (derived) | **100.0%** |
| `+4` (rival) | **100.0%** |
| `+8` (rival) | **100.0%** |
| `+12` (rival) | 0.0% |

**The control cannot fail against the rivals that matter.** Shifting the read by
four bytes gives `(y, z, nx)` — still three finite floats of modest magnitude.
The test would pass on a wrong offset, which is the definition of decoration.

**Not added.**

## Why the UV control works and this one does not

The difference is not the technique. It is **what the neighbouring field is
made of.**

```
stride 32:  POSITION(12 floats) | NORMAL(8) | COLOR(4) | TEXCOORD(8)
            +0                    +12         +20        +24
```

- **TEXCOORD's neighbour is COLOR** — four packed bytes. Reinterpreted as a float
  it is almost never finite-and-modest, so a four-byte shift collapses to 0.0%.
- **POSITION's neighbours are POSITION and NORMAL** — floats either side. A
  four-byte shift lands on more floats.

So a plausibility control is strong exactly where a field borders a
differently-encoded one, and worthless where it borders its own kind. The UV
control scored 99.8% against 0.0% not because it was well designed but because
**COLOR happens to sit next to TEXCOORD.**

That is worth writing down, because the natural generalisation — *"add a
plausibility control to every field"* — produces decoration on most of them, and
a suite full of controls that cannot fail is worse than one with none: it reads
as protection.

## What would actually protect POSITION

Neither of the two styles cycle 1241 identified is free here:

- a **snapshot control** needs an oracle, which is a milestone decision;
- a **named rival** needs a rival that fails, and the plausibility predicate does
  not supply one.

A third possibility exists and is not pursued here: a *structural* invariant —
that the union of all descriptors' vertex extents tiles `[0, [buf+0x18])` without
overlap. That constrains offsets and stride jointly and would fail under a shift.
The extent control already in the test is a weaker form of it (max extent, not
tiling). **Naming it, not building it**, because the tiling assumption has not
been measured and a control built on an unmeasured assumption is the thing this
cycle is about.

## Not established, stated plainly

- Whether descriptor vertex ranges tile the block. Unmeasured, and the reason the
  stronger control was not built.
- The `+12` column above shows only that reading twelve bytes at the NORMAL
  offset crosses into packed data. It is not evidence about NORMAL's own format,
  which cycle 1233 left open along with every other `Type` code.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
265,458 vertices; four offsets scored; three tie at 100%
```

No product code changed and **no test added**, deliberately.
