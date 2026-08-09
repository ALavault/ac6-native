# Cycle 1351 — zero vector stores

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus was read.
- No product C++ changed, no contract changed.

## The check I planned would have been circular

Cycle 1350 proposed a capsule: run `0x822A1668` on a synthetic unit with a child
whose vtable is `0x820078D0`, and see whether the three slot calls land.

They would. The dispatch reads the vtable I installed, so it dispatches wherever
I point it. **That confirms the mechanism I had already read and says nothing
about the child's class.** Caught before it was built, which is cheaper than
after.

## The non-circular check, and it comes back negative

Cycle 1340 measured the player copying `child+0x70`, `+0x80`, `+0x90` and `+0xA0`
as **four 16-byte vectors**. If the child's own slot `+0xC8` computed that pose,
it would have to write those rows.

All three slot functions, every store on every base:

| function | stores in `0x60..0xAF` | vector stores |
|---|---|---:|
| `0x82299548` (5 insns) | none | **0** |
| `0x82299560` (27 insns) | none | **0** |
| `0x8229CD78` (617 insns) | one scalar at `+0x70` | **0** |

**Zero vector stores in 1,272 instructions.** The three slots do not write the
basis the player copies. Whatever fills those four rows is somewhere else.

## Which refutes an inference of mine

Cycle 1348 wrote of `0x8229CD78`: *"617 instructions using the float sixteen
times. That is where the arithmetic went."*

That was an inference from **instruction count and register usage**, not from what
the function writes. It uses the float heavily and touches the locator only at
`+0x68`, `+0x70` and `+0x74` — two loads and one scalar store. It is doing
something with one float of row 0, not building a basis.

This is the fifth time in this thread that a surface feature — adjacency, a name,
neighbouring offsets, a slot count, now an instruction count — has been corrected
by reading what the code actually writes.

## An engineering judgement, stated rather than buried

This thread has run **eleven cycles** since A3.1 was contracted. It has produced
real, corrected, well-controlled knowledge: the transform block is a
`CGaLocator`, the player copies a pose rather than computing one, the class map
covers 73% of vtables, the child array is a caller's fifth argument into an
arena.

It has produced **no product code**, and its target has receded at nearly every
step.

The plan's rule is that a slice ends with executable code and a regression test.
A3.1 met that. A3.2 has not, and the honest reading is that "what class the
children are" is not the question that unblocks it — the flight controller can be
approached from `0x82211DF8` and the float it receives, which the plan named
first and which nothing has yet read.

## Not established

- What writes the child's locator rows.
- What class the children are. Cycle 1348's candidate is neither confirmed nor
  refuted; the check that would have confirmed it does not exist where I looked.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

Leave the child thread where it is — documented, with its one unconfirmed
candidate — and take A3.2 from the other end: `0x82211DF8` and the float it
receives, then the virtual `+0x1C` of `[0x823F6DB8]` that produces that float.
Both are named in the plan, neither has been read, and the deliverable is code.
