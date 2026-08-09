# Cycle 1420 — two cycles rediscovering a port

## Qualification

- **No Ghidra run and no oracle pass.** The product's own headers and tests, and
  one corpus search.
- No product C++ changed; ctest stays **51**. **No contract entry.**
- `tools/mdlp_index.py`'s docstring corrected.

## The correction

Cycle 1419 ended by naming the next step: *"join a Mission 01 model reference to
an MDLP index… `retail_mission_state.cpp:311` is where the two halves meet, and
it is a read before it is an edit."*

The read found the join **already ported, already tested, and derived from
retail code rather than from the file**.

`reconstruction/ace-combat-6/include/ac6/retail_model_directory.h` carries the
whole thing, from `0x8228E988` and `0x8228E9B8`:

```
8228e9d0  lwz    r11,0x4(r3)    ; the offset table
8228e9d4  rlwinm r9,r4,0x2,...  ; index * 4
8228e9d8  lwz    r10,0x8(r3)    ; the entry base
8228e9dc  lwzx   r11,r9,r11
8228e9e0  add    r3,r11,r10     ; entry = base + table[index]
```

and its own comment says, in as many words:

> Mission 01's `001_MDLP.mdlp` is one: 94 entries, table at 0x1000, base at
> 0x2000, and every entry lands on an `FHM ` signature.

That is cycle 1418's headline finding, verbatim, written before it.

And the join to mission ids is tested end to end in
`retail_model_directory_tests.cpp`: every model byte the scenario carries must
address an entry the directory serves — **311 bindings resolved, 38 distinct
primaries, 38 secondaries**, against the real payload.

`0x820A7070` indexes the directory with the bytes at `+0x61`/`+0x62` of each Obj
record, and `ScenarioModelBinding` carries them. The "top of the chain" cycle
1419 called the last missing piece was never missing.

## The thirty-first shape, for the sixth time

Two cycles derived from a 29 MB file what one `grep` of the repository would
have handed over. The shape's own rule is *"if a repository tool already answers
the question, call it"*, and I wrote the shape.

What made this instance easy to walk into, and worth recording: the plan's own
gap list says `NdxrContainer` "serves no bytes" and names
`retail_mission_state.cpp:311` as throwing the model index away. Both are true
and both are about the *wiring*. I read them as "nothing resolves models yet"
when they say "the resolution exists and the mission state does not use it."

**A gap list describes what is not connected, not what is not built.**

## What the two cycles did add, stated without inflation

- **The FHM sub-entry table** — count at `+0x10`, that many offsets from `+0x14`,
  relative to the FHM base. `ModelDirectory` stops at the entry and says so:
  *"What it does NOT do is open a file, resolve a name, or know what an entry
  contains."* This level is genuinely unported.
- **292 of 292 NDXR opened** by the product's own `NdxrContainer::Open`, once
  each span is trimmed to its declared length at `+0x04`. That the reader works
  on every real container in the mission was not established before.
- **The census inside entries**: `MATE` 381, `NDXR` 292, `NTXR` 86, 47 nested
  FHMs.
- **The even/odd pairing, killed** by its own exception check, 45 of 94.

## So the gap is exactly one hop

```
scenario record  ->  binding.primary       PORTED, tested, 311 resolved
     -> ModelDirectory.entry(id)           PORTED from 0x8228E9B8
        -> {offset, size} of an FHM        PORTED
           -> FHM[j] -> an NDXR span       NOT PORTED   <- the gap
              -> NdxrContainer::Open       PORTED, 292/292 on real data
```

One hop, with a port on either side of it.

## And it has a derivation problem worth naming before porting

A search of all 827,798 instructions for a materialisation of `0x46484D20`
(`FHM `) or a compare against it returns **zero**. Retail never checks the
signature — consistent with `retail_model_directory.h`'s own note that *"retail
trusts its own archive"*.

So the FHM sub-entry table currently has a **file-derived** reading and no retail
function behind it. Under this campaign's standard that is `static` evidence, not
a derivation, and a behaviour cannot be contracted on it. Either retail's reader
of `+0x10`/`+0x14` is found, or the port ships explicitly as a file-format reader
outside the contract — and which of those it is should be decided before the
code is written, not after.

## Not established

- Retail's reader for the FHM sub-entry table, if there is one.
- What the 47 nested FHMs are for.
- Whether `retail_mission_state.cpp:311` should carry the model index at all, or
  whether the drawing side should ask the directory directly. That is a design
  question the gap list does not settle.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
```

## Next

**Find retail's FHM sub-entry reader**, and only then port it. The search that
failed here was for the magic; the one that should work is for the *shape* — a
function loading a count from `+0x10` and indexing a table at `+0x14`, which is
the same shape as `0x8228E9B8` one level down and may well be the same family of
code. `0x820A85E0`, the vtable slot that finds the directory blob and which
`retail_model_directory.h` says is not ported, is where to start.
