# Cycle 1344 — one site in the whole image

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed; two disassemblies were
  compared.
- No product C++ changed, no contract changed.

## Enumerating a constant instead of scanning a displacement

The gate on `0x822A1668` is bit `0x4000` of `[this+0x60]`, and three cycles of
displacement scans had each produced a list. A distinctive immediate is a
different kind of question: `ori rX,rY,16384` occurs **17 times in the image**,
and exactly **one** of them is followed by a store to `+0x60` on a non-stack
base.

```
0x820A7DEC   sub_820A7070   ori r11,r11,16384  -> stw ...,96(r16)
```

**One site, not a list.** That is the fourth cycle in a row where an enumeration
answered and a scan did not.

## And `r16` touches exactly the constructor's cluster

Every field of `r16` that function touches:

```
+0x60, +0xD0, +0xD4, +0xD8, +0xDC, +0xE0, +0xE4
```

Cycle 1332 read `CAce6Unit`'s constructor initialising `+0x60` to 0, `+0xD0` to
255, and `+0xD4` through `+0xE4` to 0. **Seven offsets, seven matches, none
extra.**

That is a far stronger statement than cycle 1333's "three plausible neighbours",
and it arrived from an unrelated direction — the `0x4000` enumeration knew
nothing about the constructor.

`r16` itself is assigned by `mr r16,r3` and `mr r16,r24`, so it is a parameter or
a local, not a global.

## The stream is qualified before anything is built on it

Thirty consecutive `lwz r16,N(r10)` with monotonically increasing offsets look
like a jump table decoded as code, and `sub_820A7070` is 912 instructions — the
kind of function `CLAUDE.md` warns about.

`.pdata` declares **912**, `check_listing_against_pdata` reports `short=0`, and
the comparator now reads **912 of 912 against XenonRecomp**. Three new
equivalences were needed and each is named and counted: `clrldi → rldicl`,
`rotlwi → rlwinm`, and the `clrlwi` rule from cycle 1329 firing eight times.

**The honest limit**: two decoders agreeing means they read the same bytes the
same way. It does not mean those bytes are reached at run time, and a jump table
would be decoded identically by both. Reachability is a separate question and
this cycle did not ask it.

## Not established

- Whether `sub_820A7070` is what populates a unit. The field set matches the
  constructor's cluster exactly, which is strong; it is still a field-set match
  and not a proof of role.
- Whether the `lwz r16` run is reachable code.
- What class the children are.
- Cycle 1333's separate refutation still stands: this function is **not** the one
  that calls the player factory. Those calls are in `sub_820A7F48`.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
recomp_vs_ghidra 0x820A7070          pass, 912 instructions, 0 known defects
tools/tests                          Ran 72 tests, OK
```

## Next

`count_indirect_branches` on `0x820A7070`. If the `lwz r16` runs sit behind a
`bctr`, they are a jump table and the function is a dispatcher — which changes
what "the same function writes seven fields" means, because a dispatcher's arms
are not one path. That is cheap, it is the reachability question this cycle
declined to answer, and the tool exists because `0x82263A50` was read twice as
though it had one indirect branch when it has three.
