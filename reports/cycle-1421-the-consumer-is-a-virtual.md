# Cycle 1421 — the consumer is a virtual

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus.
- No product C++ changed; ctest stays **51**. **No contract entry** — this cycle
  narrows a search and corrects a method.

## The question

Cycle 1420 established that the only unported hop is *FHM entry → NDXR span*,
and that porting it needs retail's own reader rather than a file-format reading,
because a search for the `FHM ` magic across all 827,798 instructions returns
zero — retail never validates it.

So: **who reads an MDLP entry's contents?**

## Two steps up, and one sideways

`0x820A85E0` builds the directory. Its blob comes from `bl 0x82234DD0` with
`(x + 32, 1)`, and that callee is 13 instructions of a **generic indexed
container**:

```
count = [r3+0];  if (index >= count) return 0
table = [r3+12]; entry = table[index];  if (entry == 0) return 0
base  = [r3+4];  return base + entry
```

Note the layout: count at `+0`, base at `+4`, table at `+12`. The model
directory's is `{blob, table, base}` with the count at `blob+4`. **Two different
indexed containers**, one nested in the other, and neither is the FHM level.

## The consumers, all four of them

`0x8228E9B8` — the entry getter — has exactly **four call sites, all inside
`0x820A7070`**, the function `retail_model_directory.h` already names.

And `0x820A7070` **does not descend into the FHM.** At the site that reads the
model bytes:

```
0x820A795C  lbz r4,97(r28)      +0x61, the primary model byte
0x820A7964  bl  0x8228e9b8      -> the FHM pointer, kept in r28
0x820A7968  lbz r4,98(r28)      +0x62, the secondary
0x820A7970  cmplwi cr6,r4,255   the 0xFF sentinel
0x820A797C  bl  0x8228e9b8      -> the second pointer, kept in r29
              ... r5 = r28, r6 = r29, passed onward ...
```

and at the two loop sites:

```
0x820A718C  bl  0x8228e9b8
0x820A7190  lwz r11,1832(r30)
0x820A71A4  lwz r11,24(r10)     a VIRTUAL, slot +24
0x820A71AC  bctrl               (object, entry_pointer, 152)
```

with the second loop calling slot `+28` the same way.

**The FHM pointer is handed to virtual methods of the object at
`[r30+1832]`.** Whatever parses the entry is behind those two slots, and the
constant `152` travels with it at every site.

## Where it stops, and why that is the honest place

No `stw rX,1832(rY)` exists anywhere in the corpus, so that field is not filled
by a plain store — at one site the base itself comes from a stack slot
(`lwz r10,88(r1)`). Resolving the object needs the kind of construction trace
cycles 1370 and 1384 spent whole cycles on, and starting it at the end of this
one is how the wrong object gets identified.

What is established is the **address of the question**: two vtable slots, `+24`
and `+28`, of one object, reached from `0x820A7070`.

## The method correction, which is the part worth keeping

Cycle 1420's "next" said to search for the *shape* — a count at `+0x10` indexing
a table at `+0x14`. I ran that scan first. It returned **57 candidates** across
the corpus and not one bit of information: the pattern is too common to
discriminate, exactly as `Ac6FieldRead` on `+0x28` returned 577 candidates at
cycle 1308.

Following the **data flow** instead — who calls the getter, what do they do with
the pointer — reached the consumer in three steps and four call sites.

The recipe's own first rule is *bound the population before you scan*, and I had
written the scan into the previous report as the plan. **A structural shape is
not a population bound; a call graph is.** That is the sixth shape's lesson
arriving in a new place, and it cost the first half of this cycle.

## Not established

- The class at `[r30+1832]`, and therefore its slots `+24` and `+28`.
- What the constant `152` is. It is passed at all four sites and is not a
  length that matches anything measured so far.
- Whether the FHM sub-entry table is read by retail at all, or whether these
  virtuals consume the entry some other way. Cycle 1419 established that the
  table exists and describes the file correctly; that is a fact about the file,
  and it remains one until a function is found reading it.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
```

## Next

**Identify the object at `[r30+1832]`.** `tools/whose_vtable.py` names a class
from a vtable address, so the work is getting from the field to the vtable: find
what constructs the object `0x820A7070` reads it from, which is the same
constructor-tracing the flight thread did at cycles 1370 and 1384 and which has
a tool behind it (`tools/find_materialised_address.py` for the `lis`+`addi`
pairs a plain substring scan misses).

If those two slots turn out to be the NDXR loader rather than an FHM walker,
then the FHM table is retail's *file layout* and not retail's *code path*, and
the port ships as a file reader outside the contract — which is the decision
cycle 1420 said to take before writing the code, and it is still open.
