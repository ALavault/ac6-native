# Cycle 1341 — the unit is a shell

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed; fifty instructions were read.
- No product C++ changed, no contract changed.

## `0x822A1668` computes nothing

Fifty instructions, and every one of them is dispatch:

```
if ([this+0x60] & 0x4000) == 0        -> return
if [this+0xDC] <= 0                   -> return
for i in 0 .. [this+0xDC]-1:
    child = [ [this+0xD8] + 4*i ]
    if ([child+0x118] & 1) == 0       -> skip this child
    child->vtable[+0xC0] (child, f1, r5)
    child->vtable[+0xC4] (child, f1, r5)
    child->vtable[+0xC8] (child, f1)        <- r5 is NOT passed
```

Three consecutive virtual slots per enabled child, carrying the float. The third
call drops the extra argument, which is measured and not explained.

## Two functions agreeing about one flag word

The gate is bit `0x4000` of `[this+0x60]`. Cycle 1339 read `0x822A1F20` — the
reset at slot `+0x38` — doing `[this+0x60] &= 0x5E00`, and `0x5E00` contains
`0x4000`.

So the bit that enables this dispatch is one the reset **preserves**. Two
functions read two cycles apart, for unrelated reasons, agree about which bits of
that word survive a reset. Neither was consulted while reading the other.

## Which relocates A3.2 again

Put together with cycle 1340, the player path is:

```
slot +0x3C (player only):
    0x822A2030 (this, 1)
    0x822A1668 (this, float, r5)     -> three virtual calls per child
    copy child[0]'s locator into this+0x80
```

**On the player path the transform is not computed in `CAce6Unit` at all.** The
unit is a shell: it forwards a float to its children, then harvests the first
one's pose. The arithmetic lives behind the children's slots `+0xC0`, `+0xC4`,
`+0xC8`.

Slots `+0x34` and `+0x44` still *build* a transform from angles through
`0x822A1E80` — so a unit has both mechanisms, and which one runs depends on which
slot is called. That is worth stating plainly because a port that implemented
only the angle path would be complete-looking and wrong for the player.

## The child is a different class, twice over

Its vtable reaches at least `+0xCC` — fifty-one slots against `CAce6Unit`'s
twenty-three — and its locator is at `child+0x60` where a unit's is at `+0x80`
(cycle 1340). Two independent layout differences, from two different cycles.

## Not established

- What class the children are.
- What the three slots do — this is now the arithmetic's location and it is
  unread.
- What the float is. It is **not** called delta time here, and eight cycles of
  this thread have avoided naming it.
- What `r5` is, and why the third call drops it.
- What `0x822A2030(this, 1)` does.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

The children. Their class is identifiable the way `CGaLocator` was — bound the
population, then look the vtable up in the class map rather than inferring it.
The signature to bound on is concrete: a locator at `+0x60`, a flag word at
`+0x118`, and virtual slots at `+0xC0`…`+0xC8`.
