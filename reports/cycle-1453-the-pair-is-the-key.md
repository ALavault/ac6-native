# Cycle 1453 — the pair is the key

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the ported decoders.
- Product C++ **changed**: three assertions added to the placement test; the
  derivation header corrected. ctest stays **56**.
- Contract: `retail_map_placement`'s statement and native-test claim corrected.
  Still **34 behaviours**.

## The test cycle 1452 predicted would work

> "Render the city selecting with `tag & 0xFFFF` and again with the nine-bit
> selector; the wrong one puts towers where sheds belong and roads on end."

Both were drawn from the same viewpoint. The nine-bit choice tiles **identical
silos** across the inland terrain and marches **identical warehouses in a regular
row out over the bay** — 1,156,253 triangles from 113 models against 331,765 from
129. `tag & 0xFFFF` gives the varied coastal city of the earlier captures.

So the picture points at `tag & 0xFFFF`. It is still only a picture, and this
cycle found the reason it cannot be promoted.

## The instrument, refuted a second time

The footprint-over-water measure that failed to resolve a rotation sign was
extended to this question, where the two readings differ by whole buildings:

```
tag & 0xFFFF   15347 of   873524   1.757%
nine-bit       15968 of  1412104   1.131%
```

It favours the nine-bit field — and the denominators are 0.87 M against 1.41 M
samples, because the nine-bit choice picks **larger models**. The measure is
tracking model size, not correctness. Second question, second refutation, same
instrument; it is not a blunt tool but a wrong one, and both cycles are on record
rather than one of them quietly reporting the number it liked.

## What is actually established

The pair `(tag & 0xFFFF, selector)` is **unique across all 4,226 accepted
instances**. 173 distinct low values, 160 distinct selectors, 27,680 possible
pairs — and over 4,226 draws chance predicts about **323 collisions**. There are
zero.

> Together they are an instance key. Separately, neither is a model id.

The test asserts that against the predicted collision count, because "all
distinct" means nothing until you know how surprising it is.

## Which corrects cycle 1452, by half

That cycle read `0x82102378` handing the nine-bit field to vtable slot `+0x5C` —
`this->table[0x1B63 + index]`, bounds-checked — and concluded "the nine-bit field
is retail's part selector". The first half is read from instructions and stands.
The second half quietly assumed that retail's **runtime table** and the
container's `%03u_NDXR.ndxr` files share an order. Nothing says they do. The
table is built by a loader this campaign has not read.

So drawing with either field is a guess, and the renders from cycle 1449 onward
are recorded as **unjustified in their model choice**. They are not withdrawn —
the placement, the ground and the geometry decode are each contracted — but which
building stands where is not.

## Not established

- Which field names a model, and in what order the resource table is filled.
- What `tag & 0xFFFF` is for; `0x821023B4` extracts it and this cycle did not
  follow it.
- `this+0x6D8C`, `this+0x74`.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
contract_addresses                    pass cited=320 supported=320
contract_derivations                  pass behaviours=52 gaps=0 multiple=0
tools/tests                           Ran 79 tests, OK
```

## Next

**Read the loader that fills `this+0x6D8C`.** It is the only thing that can map
the nine-bit selector to a file, and it is a bounded search: `0x820FBC28` is the
map loader and the table's count sits at `this+0x74`, so a scan for stores to
`+0x74` inside `CMapManager`'s own code has the shape that worked at cycle 1448
and cost 51 candidates rather than 4,504.
