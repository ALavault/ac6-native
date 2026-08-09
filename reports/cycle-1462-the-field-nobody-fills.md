# Cycle 1462 — the field nobody fills

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py` and
  `.pdata`.
- No product C++ changed; ctest stays **56**. **No contract entry.**

## Correcting a reading I made this cycle

The scan for writers of `CMapManager+0x30` returned four sites in the class's
code, and I called `0x820F9F4C stw r6,0x30(r3)` a setter taking its value from a
parameter.

It is not. Two instructions earlier:

```
0x820F9F30  lis   r10,0x44
0x820F9F38  ori   r6,r10,0x3C40      r6 = 0x00443C40, a constant
0x820F9F4C  stw   r6,0x30(r3)
0x820F9F64  ori   r5,r10,0x3834      and 0x004B3834 at +0x3C
```

`+0x3C` on `CMapManager` is the `.mcd` pointer; here it takes a literal. So `r3`
is **not** a `CMapManager`. `.pdata` puts `0x820F9F4C` inside `0x820F9E78`
(78 instructions), and the constructor calls that as
`Function_820F9E78(puVar8 + 0x1e55b)` — a **sub-object at `this + 0x7956C`**.

That is *the displacement collision*, indexed in `INSTRUMENT_DISCIPLINE.md`
since long before this cycle, with the remedy spelled out: read the four lines
around each hit. Reading them took thirty seconds and I published the wrong
reading first anyway.

## What the field does

```
0x8210218C  lwz    r7,0x30(r31)
0x82102190  cmpliw cr6,r7,0x0
0x82102194  bc     12,26,0x8210254C      -> abandon the whole draw
```

`0x82102148` reads `.pdl` at `+0x28`, then reads `+0x30`, and **bails when it is
zero** — before the size check, before the coarse bounds, before anything. The
entire map-part draw path is gated on it.

And it is an indexed table: `0x821021D8 lwzx r11,r11,r7` with the coarse index
times four, then the entry added to `r7` itself and offset by `(cell & 15)` times
eight. So `+0x30` points at 256 u32 offsets, each reaching 256 eight-byte records
— one per sub-cell.

## The bounded negative

Inside `0x820F9000..0x82110000`, the class's own code, `+0x30` is written **four
times**: twice by the constructor and its destructor helper, both storing zero;
once at `0x820F9F4C`, which is the sub-object above; once at `0x82106344` on a
different base register, outside the functions this campaign has attributed to
`CMapManager`.

**Nothing in the class fills it.** The loader `0x820FBC28` writes `+0x28`,
`+0x2C`, then `+0x34`, `+0x38`, `+0x3C` — stepping over `+0x30` in the middle of
its own sequence.

Image-wide there are **414** `stw rX,0x30(rY)` sites, and I have discriminated
none of them. So the negative is bounded to the class and stated that way: not
"nobody writes it", but "no function this campaign has attributed to
`CMapManager` writes it".

## Not established

- Who writes `+0x30`, and therefore whether the `.pdl` draw path runs at all in
  a normal frame.
- What the 256 eight-byte per-sub-cell records are.
- `0x82106344`, the fourth site.

## Gates

```
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 56
microexec_reset_completeness            pass fields=27 constants=6 missing=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Find the accessor that hands out a `CMapManager*`.** 414 undiscriminated
writes is the population problem the plan names, and the way through it is the
one that worked at 1448: bound first. The three mission managers embed the class
at fixed offsets — `+0x35C00` and `+0xAFDA0` in the campaign one — so a scan for
the `addis`/`addi` pair that forms those offsets finds every function that can
hold such a pointer, and the writer is among them or the field is set through
one of them.
