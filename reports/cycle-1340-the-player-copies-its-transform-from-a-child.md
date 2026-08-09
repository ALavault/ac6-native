# Cycle 1340 — the player copies its transform from a child

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed; 42 instructions were read.
- No product C++ changed, no contract changed.

## The one thing a player unit does that a plain one does not

`0x822A6710`, slot `+0x3C`, is 42 instructions. After two calls and a guard on
the child count, its whole remaining body is a **locator copy**:

```
r9  = this + 0x80          the unit's own CGaLocator
r11 = [this + 0xD8]        the child pointer array
r11 = [r11 + 0]            child[0]
r8  = child + 0x60         the CHILD's locator — a different offset

[child+0x70] -> [this+0x90]     row 0
[child+0x80] -> [this+0xA0]     row 1
[child+0x90] -> [this+0xB0]     row 2
[child+0xA0] -> [this+0xC0]     translation
```

**A player unit does not compute its transform here. It takes its first child's.**
Four `lvx128`/`stvx128` pairs, no arithmetic between them.

That is a different mechanism from everything this thread has read so far:
`+0x34` and `+0x44` *build* a transform from angles through `0x822A1E80`; `+0x3C`
*copies* one wholesale, and only the player kinds have it.

## Three confirmations from a direction that never runs a rotation

This function does no vector arithmetic at all, which makes it an unusually clean
witness for three things derived elsewhere:

**The locator's payload is exactly four 16-byte rows at `+0x10`…`+0x4F`.** A
function whose entire job is to copy one copies precisely those four and nothing
else.

**It starts at `+0x10` and never touches `+0x00`.** Cycle 1335 established that
word is the vtable pointer and that writing it would destroy the object's type.
Here is a copier that agrees without being asked — the strongest kind of
corroboration, from code that had no reason to care.

**The translation is the fourth row and travels with the basis as one block.**
Cycle 1330 derived that from lane-3 values, cycle 1332 from the constructor's
`(0,0,0,1)`. This is a third derivation, from a bulk copy, and the three share no
reasoning.

## The child is not a unit

Its locator is at `child+0x60`. A `CAce6Unit`'s own is at `+0x80`, and it has a
second at `+0x10`. **The layouts differ, so the child is a different class**, and
nothing read so far says which.

That also gives the child array a purpose: `[this+0xD8]` with count `[this+0xDC]`
is not bookkeeping — the first element supplies the unit's pose.

## Not established

- What `0x822A2030(this, 1)` and `0x822A1668(this, f1, r5)` do. They run before
  the copy and the float goes to the second.
- What the float is. It is **not** called delta time here.
- What class the children are.
- Whether elements past `child[0]` are ever used for this.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

`0x822A1668` — it receives the float and runs immediately before the copy, so it
is the natural place for whatever the float means. And it is the first function
in this thread that is both player-specific and takes a per-frame-looking
argument, which is what A3.2 has been walking towards for eight cycles.
