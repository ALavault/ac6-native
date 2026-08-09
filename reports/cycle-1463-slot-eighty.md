# Cycle 1463 — slot eighty

## Qualification

- **No Ghidra run and no oracle pass.** The image, `.pdata`, the class map.
- No product C++ changed; ctest stays **56**. **No contract entry.**

## Bounding 414 down to 27

Cycle 1462 ended with 414 undiscriminated writes to displacement `0x30`. The
three mission managers embed `CMapManager` at fixed offsets, so the pointer must
be formed as an `addis`/`addi` pair against those constants. A scan for exactly
those pairs returns **27 sites** — three families of nine, one per mission-manager
template, each doing the same things in the same order.

**None of the 27 writes `+0x30`.** Every one hands the pointer straight to a
call: `0x820FA258` (the constructor), `0x820FA6F0` (the destructor),
`0x820943B0` and `0x82222F20` (both maps), and `0x82105738` (the first map, in
all three families). `0x82105738` is 219 instructions and its only small stores
are at `0`, `4`, `8`, `0xC` off a list node.

So nothing that can hold a `CMapManager*` from a mission manager fills `+0x30`.

## And the worry that produced, which was wrong

`0x82102148` and `0x82102568` are called only by `0x82102E70`, and `0x82102E70`
has **no callers**. A field nothing writes, gating a path nothing calls, reads
like dead code — the shape `INSTRUMENT_DISCIPLINE.md` indexes as *the true
positive from dead code*, where four findings this campaign were live code the
build never reaches.

It is not dead. `0x82102E70` appears once as an aligned word, at `0x8205CAE4`:

```
0x8205CAE4 − 0x8205C9A4 = 0x140
```

> **Slot 80 of `CMapManager`'s own vtable.**

`tools/whose_vtable.py` reported it as unnamed on the first run, and named on the
second. It walks back **64 words** looking for the RTTI locator, and this vtable
is **81 slots** — `0x140` is 320 bytes, past the default. `--span 100` names it
immediately. The same walk-back covers the loader `0x820FBC28` at slot 58 and the
destructor helper at slot 21; every slot from `0x8205CA80` to `0x8205CAE4` is a
function in this class's own address range.

**Reachability by `bl` covers about a quarter of this program**, which is the
first thing that file says about negatives, and I built a dead-code hypothesis on
one anyway before checking the vtable.

## Where that leaves `+0x30`

Unresolved, and now sharper: the draw path is **live**, it is slot 80, and it
abandons on a zero `+0x30` that no reachable writer sets. Either the writer is
outside the 27 — reached through a pointer the mission managers hand on, which
`0x82105738` and `0x820943B0` both do — or the gate is a mode this build never
enters.

Nothing about cycles 1445–1454 changes. Those read layouts out of instructions,
and an instruction's meaning does not depend on the function being called.

## Not established

- The writer of `+0x30`.
- What the 256 eight-byte per-sub-cell records behind it are.
- Whether `whose_vtable.py`'s 64-word default has silently mis-answered earlier
  cycles. It is the tool that named `CX360UnitManager`, `CX360ActorModelSetup`
  and `CMapManager` itself, and a slot past 256 bytes would have come back
  unnamed each time.

## Gates

```
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 56
microexec_reset_completeness            pass fields=27 constants=6 missing=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Raise `whose_vtable.py`'s default span and re-run it on every address this
campaign named with it.** The question in *not established* is cheap to answer
and expensive to leave: three class names rest on that tool, and this cycle
showed its default silently returns "unnamed" for a slot 320 bytes into a
vtable. A re-run either confirms three findings or corrects them.
