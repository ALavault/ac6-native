# Cycle 1342 — I read the path that was not taken

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## The correction, inside the cycle

`0x822A6710` calls `0x822A2030(this, 1)`. Cycle 1341 quoted that function's
opening and I began reading its body straight through — but the third
instruction after the prologue is

```
clrlwi r10,r4,24 ; cmplwi cr6,r10,0 ; bne cr6,0x822A2124
```

and the call site passes **1**, so the branch is taken and everything I had in
front of me was the path that does **not** run. Caught by checking the argument
against the guard before reading further, which cost one glance and would have
cost a published paragraph.

## What the taken path does

It is a **range-band classifier** writing into `[this+0x60]` — the same flag word
the child dispatch gates on:

```
f13 = [[global] + 0x264] ? *that : 24000.0
if f30 > f13 : clear bit 0x04   else : set bit 0x04
if f0  > 12000.0 : (skip)       else : set bit 0x20
if f0  >  6000.0 : (skip)       else : set bit 0x10
```

The three thresholds were read, not inferred: **24000.0**, **12000.0**,
**6000.0** — a halving ladder, and the first has a global override.

**The dispatch gate is not among them.** `0x822A1668` gates on bit `0x4000` of
this same word, and this function writes `0x04`, `0x20` and `0x10`. So the
pre-call does not enable the dispatch; it annotates the same word with something
else.

## A claim I will not finish by reading

On this path both compared registers trace to the same source: the prologue does
`lfs f0, 2092(r11)` — the `0.0` at `0x8200082C` — and `fmr f30,f0`, and the
branch jumps past everything between. In the instructions read, nothing
reassigns either before the comparisons.

If that holds, all three comparisons are `0.0 > threshold`, all false, and
`0x822A2030(this, 1)` **sets a fixed bit pattern regardless of anything**.

That is a strong enough conclusion that I am not willing to reach it by reading.
It is exactly what the micro-execution harness settles in one run — seed `r4 = 1`,
execute, read `[this+0x60]` — and that is a better use of the next cycle than
more staring.

## The player constructor creates no child

`0x822A6560`, 19 instructions, whole: chain to `0x822A2330`, zero `[this+0xF0]`,
install the player vtable `0x820568D4`, call the reset `0x822A1F20`, return.
Nothing is allocated and no child is attached. So the child array is filled by
something else.

## And a third scan that produced a list

Functions writing **both** `+0xD8` and `+0xDC` on the same non-stack base — the
signature of an add-child routine — number **17**, and one of them is the
constructor writing zeros. Same collision as `+0xE0` and `+224`, narrower and
still a list.

Three cycles running, a scan has produced candidates and an enumeration has
produced answers. That is now a pattern rather than an accident, and the next
question should be chosen for whether it can be *enumerated*.

## Not established

- Whether `0x822A2030(this, 1)` really reduces to a constant.
- What fills the child array.
- What class the children are.
- What the float is.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

Micro-execute `0x822A2030` with `r4 = 1` and read `[this+0x60]`. It is one
capsule, it settles the constant question outright, and it is the first thing in
nine cycles of this thread that can be *run* rather than read — the instrument
exists precisely so that reading stops where measuring can start.
