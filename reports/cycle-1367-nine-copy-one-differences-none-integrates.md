# Cycle 1367 — nine copy, one differences, none integrates

## Qualification

- **No Ghidra run beyond `.pdata` checks, and no oracle pass.**
- No product C++ changed, no contract changed.

## The population of ten, read completely

Cycle 1366 reduced the integrator search to ten functions that read a locator's
translation row and write it back. Classified by what sits between the load and
the store:

```
nine functions   copy-only -- no vector arithmetic at all
one function     sub_8232E168, a vsubfp128
```

And the one with arithmetic is not an integrator either:

```
8232e548  addi r11,r31,32          this + 0x20
8232e54c  li   r10,64
8232e550  addi r30,r31,16          this + 0x10
8232e558  lvx128 v0,r0,r11
8232e55c  vsubfp128 v0,v127,v0     v127 - [this+0x20]
8232e560  stvx128 v0,r31,r10       [this+0x40] = the difference
8232e564  stvx128 v127,r0,r30      [this+0x10] = v127
8232e568  stvx128 v127,r0,r11      [this+0x20] = v127
```

A **finite difference**: the new position minus the previous one goes to `+0x40`,
and both stored copies are updated. It derives motion **from** positions rather
than positions from motion — the inverse of an integrator.

## A complete negative over a defined population

Of every function in this image that reads and writes a locator translation row
through the `li rN,64` form, **none integrates**. Nine copy a pose and one
differences two.

That is a real result rather than a failure to find one, and it fits what this
thread has already measured: cycle 1340 found the player unit **copying** its
pose from a child rather than computing it, and cycle 1366 found nine more
copiers. Poses move around this engine by copying far more than by being built.

## What it rules out, and what it leaves

**Ruled out**: an integrator that writes a translation row as a vector, through an
index register set to 64. That was the whole `stvx128` form the campaign has seen
locators written with, in every function read since cycle 1327.

**Left open**: the same field written as three scalar `stfs` at `+0x40`, `+0x44`,
`+0x48`. Nothing in this thread has looked for that, and it is a different
enumeration with a different distinctive constant — a multiply-add on floats
whose destination displacement is one of those three.

## Not established

- The integrator.
- What `sub_8232E168` is; it is 651 instructions and only its delta step was read.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

The scalar form: `stfs` to displacements `0x40`, `0x44` or `0x48` on a base that
also feeds an `lfs` from the same displacement, with a `fmadds` or an `fmuls`
between. If that also comes back empty, the assumption to revisit is not where the
integrator is but whether a locator's translation is what it writes at all.
