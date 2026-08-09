# Cycle 1282 — both family labels are attached, and why prefixes lie

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; `analysis/class-map.tsv` (811
vtables, RTTI-derived, J2-gated). `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## Established — the second label fails the same test

Cycle 1281 showed `0x820078D0` is not `galib::CGaObj` (whose vtable is
`0x820572C0`) and left the question of whether `0x82009440`, "the unit family",
is mislabelled the same way. It is.

```
class map:  ACE6::CAce6Unit  ->  vtable 0x82056874
0x82056874 vs 0x82009440 over 96 slots:  9 identical, 87 different
```

| family | the campaign's label | the class map's vtable | shared of 96 |
|---|---|---|---:|
| `0x820078D0` | `galib::CGaObj` | `0x820572C0` | 11 |
| `0x82009440` | `ACE6::CAce6Unit` | `0x82056874` | 9 |

Both tables carry no RTTI locator — `0x820078D0` holds zero at `−4`,
`0x82009440` holds a function address — and neither is in the audited map.

## Established — why a prefix comparison misleads here, measured

Cycle 1281 was misled by five shared slots. The reason is structural and it can
be counted:

- `0x822663A8` is a `li r3,0 ; blr` stub. It occupies slot `+0x04` in **27** of
  the 811 named vtables and slot `+0x08` in **24**.
- On `0x82056874` it fills `+0x04`, `+0x08`, `+0x0C`, `+0x10` and `+0x14`
  outright — five consecutive slots of the same stub.
- **261 of 811 named vtables share their `+0x04`…`+0x14` with at least one
  other.**

So roughly a third of the named population is indistinguishable from something
else on a five-slot prefix. A comparison over six slots is not weak evidence of
relatedness here; **it is no evidence at all**, because the early slots are
where a compiler puts the empty virtuals that every class inherits and few
override.

## What changes and what does not

**No structural fact moves.** The two families were distinguished by
`subi r3,r10,0xf0` against `subi r3,r10,0x268`, by their factory slots `+0x14`
and `+0x10`, and by the field offsets read out of `0x820A7070`. None of that
came from a class name, and none of it is affected.

**Every sentence naming a class by these two vtables is imprecise.** The honest
form is "the object whose vtable is `0x820078D0`" and "…`0x82009440`", which is
what was measured. Where a name is wanted, the map has 811 that are read rather
than attached.

## Not established

- **What either class is.** Nine and eleven shared slots put a common ancestor
  several levels up; they do not name it.
- Whether other labels in the reports rest on the same attachment. Two were
  tested because two were used constantly; the rest were not swept.
- Whether the campaign's earlier naming used a different method than a prefix
  comparison. Cycle 1281 guessed it did not; that is still a guess.
