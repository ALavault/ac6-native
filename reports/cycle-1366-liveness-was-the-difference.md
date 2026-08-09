# Cycle 1366 — liveness was the difference

## Qualification

- **No Ghidra run beyond a `.pdata` check, and no oracle pass.**
- No product C++ changed, no contract changed.

## The enumeration, and what made it work

Cycle 1365 found the naive integrator signature matched **461** functions. The
sharper form — `li rN,64` feeding a `stvx128` whose base also feeds an `lvx128` —
gave **34**.

Nineteen of those thirty-four had base `r0`. In `stvx128 vD,rA,rB` the effective
address is `(rA|0) + rB`, so with `rA = r0` the index register **is** the whole
address, and "the register holding 64" would mean a store to address 64. Absurd
on its face, and the tell that the filter was wrong: it collected every
`li rN,64` anywhere in the function without checking the register still held 64
at the store.

That is the same failure as cycle 1330's `f25` — a register read as constant
because only its first write was looked at.

Tracking liveness forward — clearing a register on any write, and clearing the
volatiles across every call — gives **10**.

```
461  a vector load, a multiply-add and a store
 34  plus an index register that was set to 64 somewhere
 10  plus that register still holding 64 at the store, and a base that is not r0
```

## The first one read is a locator copy, on the child's layout

`sub_8229BE98`, 84 instructions, `.pdata` agreeing:

```
r10 = this + 0x60          a locator
r30 = this + 0x10          another locator
[r10+0x10] -> [r30+0x10]   row 0
[r10+0x20] -> [r30+0x20]   row 1
[r10+0x30] -> [r30+0x30]   row 2
[r10+0x40] -> [r30+0x40]   translation
```

then two floats at `this+0x50` and `this+0x58` are clamped between two `.rodata`
constants, and on one branch the same four rows are copied again into an object a
virtual call returns.

**It is a copy and a clamp, not an integrator.** But the layout is the finding:
locators at `+0x10` **and `+0x60`**.

## Which corroborates cycle 1348's candidate, from a new direction

Cycle 1340 measured the child's locator at **`child+0x60`** — the offset that
distinguishes it from a `CAce6Unit`, whose own is at `+0x80`. Cycle 1348 named one
unnamed vtable, `0x820078D0`, as the only candidate matching both measured child
properties, and could not confirm it.

`sub_8229BE98` has a locator at `+0x60`, and it sits at `0x8229BE98` — in the same
few kilobytes as `0x82299548`, `0x82299560` and `0x8229CD78`, the three slot
functions of that very vtable.

That is not proof. It is a second, independent line arriving at the same class
shape without using the class map, the arena or the vtable enumeration — and it
came from a signature search that knew nothing about any of them.

## Not established

- The integrator. Nine of the ten remain unread.
- The child's class, still.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

The other nine. The list is small enough to read completely — the size this
thread has learned to prefer, and the first time the integrator search has had a
population rather than a candidate list.
