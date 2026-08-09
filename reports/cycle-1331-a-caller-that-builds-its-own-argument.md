# Cycle 1331 — a caller that builds its own argument

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## The population was bound before anything was scanned

Cycle 1330 ended with `r30` unnamed and a warning about naming it anyway. The
recipe says bound the population first, and here that took three facts:

- `0x822A23D8` does `mr r30,r4`, so the structure is **passed in**, not a member;
- it has exactly **three** direct callers;
- `FindDataPointersTo` finds exactly **one** data reference to it — a `.pdata`
  row at `0x82082E00`, which the tool labels as `.pdata` rather than leaving me
  to mistake it for a dispatch slot. `CLAUDE.md` names that trap by name and the
  tool already avoids it.

So it is **not** a vtable slot, and three call sites are the whole population.
`Ac6FieldRead` on an offset would have returned hundreds of candidates and no
information — cycle 1308 measured exactly that cost.

## And then one caller answered outright

`0x822A2B50` is 38 instructions and it **builds the block on its own stack** at
`r1+80`, then calls. Every field is a store with a visible source, so the layout
is read rather than reconstructed from uses:

| offset | value |
|---|---|
| `+0x00` | 0.0 |
| `+0x04` `+0x08` `+0x0C` | `[caller_r4 + 0]`, `+4`, `+8` — the vector that reaches `transform+0x40` |
| `+0x10` `+0x14` | 0.0 |
| `+0x18` | the caller's **f1** — the angle applied second, about basis row 0 |
| `+0x1C` | the caller's **f2** — the angle applied first, about basis row 1 |
| `+0x20` | 0.0 |
| `+0x24`…`+0x2E` | `0x0000`, `0xFFFF`, then bytes `00 FF 00 00 FF FF 00` |

**It is a per-call argument block, not a persistent object.** That is why it has
no vtable, and it is why this file still does not call it a state, a pose or a
unit — those words would add nothing the stores do not already say.

The byte at `+0x2A` is the selector `0x822A23D8` branches four ways on
(`lbz r11,42(r30)`), and `0x822A2B50` sets it to **0**.

And the third angle does not travel in the block: `0x822A2B50` does `fmr f1,f3`,
so `0x822A23D8`'s own `f1` carries it while the first two arrive at `+0x18` and
`+0x1C`.

## The other small caller does not build one

`0x822A2B08` walks a block out of a container instead:

```
r9  = [r3 + 0xE0]
r11 = [[r9 + 4] + r4 * 8]
r11 = [[r11 + 4] + 4]
r4  = [r11 + 0]
f1  = 0.0
```

Two pointer hops and an 8-byte stride indexed by the caller's own `r4`. **What
that container is has not been established**, and it is the natural next thread —
it is the first thing in this chain that looks like it owns something rather than
being handed one.

## Not established

- What `+0x24`…`+0x2E` mean, and what the four branches on `+0x2A` do.
- What `0x82296E40` — 205 instructions, the third caller — passes.
- The container behind `r3+0xE0`.
- Nothing in the product changed. A3.2 still has a map and no code.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

The container at `r3+0xE0`, and `0x82296E40`. Then A3.2 has enough to port
something: the argument block is a plain struct, its two angles and its vector
are placed, and the kernel below it is already contracted.
