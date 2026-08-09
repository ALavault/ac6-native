# Cycle 1345 — the runs were the tables

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## The caveat was right, and it is now confirmed

Cycle 1344 qualified `sub_820A7070` at 912 of 912 against XenonRecomp and wrote a
limit it could not close: *"two decoders agreeing means they read the same bytes
the same way. It does not mean those bytes are reached at run time, and a jump
table would be decoded identically by both."*

`count_indirect_branches` closes it:

```
bctr=5  bctrl=30
  0x820A72E8  bctr   table 0x820A72EC
  0x820A73D0  bctr   table 0x820A73D4
  0x820A75D4  bctr   table 0x820A75D8
  0x820A7728  bctr   table 0x820A772C
  0x820A7834  bctr   table 0x820A7838
```

The five table addresses are **exactly** where cycle 1344 saw runs of
`lwz r16,N(r10)` with monotonically rising offsets. Those runs are the jump
tables. Both decoders read them identically because both are decoding bytes, and
neither was wrong — the bytes simply are not instructions.

`sub_820A7070` is a **dispatcher**: five jump tables and thirty indirect calls in
912 instructions.

## Which weakens yesterday's field match, and I am saying so

Cycle 1344 found `r16` touching exactly the seven offsets `CAce6Unit`'s
constructor initialises, and called it "far stronger than three plausible
neighbours". The seven-for-seven match stands — the jump-table bytes decode with
offsets like 29440 and never entered that set.

But **"the same function" is not "the same path"**, and in a function with five
jump tables that distinction is the whole question. The writes sit at three
separate places:

```
0x820A7648  +0xD0     0x820A7C74  +0xD8      0x820A7DF4  +0x60
0x820A764C  +0xD4     0x820A7C78  +0xDC      0x820A7E00  +0x60
                      0x820A7C80  +0xE0      0x820A7E2C  +0x60
                      0x820A7C88  +0xE4
```

The `+0xD0`/`+0xD4` pair is separated from the rest by **two** jump tables
(`0x820A772C` and `0x820A7838`). Nothing shows those groups are ever reached in
one execution.

So the claim shrinks from *"this function populates a unit"* — which cycle 1344
was careful not to make, and which I would otherwise have drifted into — to
*"every field this function writes through `r16` belongs to the unit's
initialised cluster, across at least two dispatch arms"*.

## And `+0x60` is written three times, not once

Cycle 1344 reported the single `ori …,16384` site followed by a store. There are
**three** stores to `+0x60` in that tail, at `0x820A7DF4`, `0x820A7E00` and
`0x820A7E2C`. Only the first follows the `0x4000` set; what the other two write
was not read.

## Not established

- Which arms reach which writes.
- Whether any single execution writes both groups.
- What the thirty indirect calls dispatch to.
- What class the children are — the question this thread has been circling for
  five cycles.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

This thread has spent six cycles descending into `sub_820A7070` and the answer it
wants — what class the children are — is not down there. The children are reached
as `[[unit+0xD8] + 4*i]` and called through slots `+0xC0`…`+0xC8`; a vtable with
a real function at `+0xC0`, `+0xC4` **and** `+0xC8` is an enumerable property of
the 811 named vtables in the class map, and enumeration has answered four times
running where scanning has not.
