# Cycle 1339 — an override that overrides nothing

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## The correction, to yesterday

Cycle 1338 wrote: *"the specialisation is at `+0x38` and `+0x3C`."* Reading the
functions instead of the table, **`+0x38` is not a specialisation**.

`0x822A6500` is **one instruction**:

```
b 0x822A1F20
```

A jump to the base class's own implementation, with `this` unchanged and no
adjustment. Both derived classes install it, so slot `+0x38` reaches exactly the
base's code through one extra branch. A vtable diff cannot tell that from a real
override, and I published it as one.

## And `+0x04`/`+0x0C` are constants, not behaviour

The three functions the vtables are mostly made of, read whole:

| function | body |
|---|---|
| `0x822DDBE8` | `blr` — does nothing |
| `0x822663A8` | `li r3,0 ; blr` — returns 0 |
| `0x82266390` | `li r3,1 ; blr` — returns 1 |

So `CAce6UnitPlayer` at `+0x04` and `CAce6UnitOtherPlayer` at `+0x0C` replace a
**constant false with a constant true**. They are a pair of boolean type-queries,
each answered `true` by exactly one of the three classes and `false` by the other
two.

They are *not* named here. "A predicate that only the player answers true" is
measured; what the caller asks it is not.

## Which leaves exactly one real specialisation

`+0x3C`. The base has the do-nothing `blr`; both derived classes install
`0x822A6710`, which has a real prologue and takes a **float** in `f1`.

That is the whole behavioural difference between a plain unit and the two player
kinds: **one method, taking one float, that the base does not implement.**

## What the base's `+0x38` actually does

`0x822A1F20` is the slot `CAce6Unit`'s constructor calls at its last
instruction, so it is the reset path:

```
[this+0x60] &= 0x5E00
if [this+0xDC] > 0:
    for i in 0 .. [this+0xDC]-1:
        r3 = [ [this+0xD8] + 4*i ]
        virtual call through slot +0xAC on r3
bl 0x822A1AB8 (this)
[this+0xE8] = 0.0 ; [this+0xEC] = 0.0
```

**`+0xD8` is a pointer array and `+0xDC` is its count** — a unit owns children
and forwards the reset to each through their slot `+0xAC`. The constructor zeroes
`+0xD4`, `+0xD8`, `+0xDC`, `+0xE0` and `+0xE4` together (cycle 1332), so `+0xE0`
— the container `+0x34` walks — sits inside that same cluster of five words, and
`+0xE8`/`+0xEC` are two floats this reset clears.

## Not established

- What `0x822A6710` does, and what its float is.
- What the child slot `+0xAC` is, and what class the children are.
- What writes `+0xD8`, `+0xDC` and `+0xE0`.
- What the two predicates are asked for.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

`0x822A6710` — the one genuine specialisation, one function, taking one float.
It is also the first thing in this thread that is *specific to a player-controlled
unit*, which is what A3.2 is looking for.
