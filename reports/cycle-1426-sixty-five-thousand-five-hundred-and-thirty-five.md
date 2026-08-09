# Cycle 1426 — 65535

## Qualification

- **No Ghidra run and no oracle pass.** The product's ports run over the
  extracted package.
- No product C++ changed; ctest stays **52**. **No contract entry** — this
  establishes the addressing the next cycle decodes with.
- New: `tools/ndxr_offset_arbitration.cpp`; `tools/ndxr_geometry_census.cpp`
  extended to reconcile.

## The census is exhaustive

Cycle 1425 flagged that its 1227 counted only successful `Descriptor()` calls, so
a silent refusal would read as a smaller total rather than an error. Reconciled
against the containers' own declarations:

```
containers opened 292
  records:     declared 1041  served 1041  REFUSED 0
  descriptors: declared 1227  served 1227  REFUSED 0
  records flagged relocated: 0
```

Nothing is being dropped. The denominator holds, and the `relocated` flag —
which the container's header says is expected false on disk — is false 1041
times.

## Where the geometry actually is

Both offsets arbitrated by cross-match over all 1227 descriptors:

| | | |
|---|---:|---|
| **vertices** at `sections.second + vertex_offset` | **1227 / 1227** | positions finite and in range |
| the same at `sections.first` | 974 | |
| the same at the file base | 907 | |

| | | |
|---|---:|---|
| **indices** at `sections.first + index_offset`, **relative to the descriptor's own vertices** | **1227 / 1227** | |
| the same, absolute from `vertex_offset / stride` | 292 | refuted |

`index_offset` is a **byte** offset, and `>> 1` gives the StartIndex the header
records from `0x823648C4`. Both arrays accumulate: descriptor 1's
`vertex_offset` is 37604, exactly descriptor 0's vertex bytes, and its
`index_offset` is 5164, exactly descriptor 0's index bytes.

The layout falls out of the section values and is checkable by hand: in the first
container, indices run from 784 to 6738 and `sections.second` is 6752.

## 65535

The index test failed three times running — 17, 42, 14; then 42, 13, 42 — while I
tried a new hypothesis each time. The fourth attempt printed sixteen `u16`s:

```
23 24 22 25 65535 56 54 55 43 53 39 36 40 37 18 20
```

**`0xFFFF` is a strip-restart sentinel.** Every range test had been rejecting it
as an out-of-range index. Honouring it, the relative reading goes from 42 to
**1227 of 1227** and the absolute one stays refuted at 292.

The values on either side of it are exactly what triangle strips look like:
`23 24 22 25`, restart, `56 54 55`, and so on.

## The method note, because this is twice now

Cycle 1421 recorded that a structural scan returned 57 candidates and no
information while following the data flow answered in three steps, and called it
*"a structural shape is not a population bound; a call graph is."*

This cycle spent three rounds proposing readings of a byte range I had not
looked at. **Sixteen `u16`s printed once settled what three arbitrations could
not.** The arbitration method is right and it was applied too early: it
discriminates between hypotheses, and I had no hypothesis containing a sentinel
because nothing had shown me one.

Look at the bytes before enumerating readings of them. The arbitration then has
something real to choose between — which is exactly how it worked once the
sentinel was in the model.

## What is now established, end to end

```
binding.primary -> ModelDirectory.entry(id)           contracted
   -> ContainerIndex over the FHM                     contracted (1423)
      -> NdxrContainer::Open on array1's length       contracted
         -> Record -> Descriptor
            vertices: sections.second + vertex_offset, stride T8[hi]+T18[lo]
            indices : sections.first  + index_offset, u16, restart 0xFFFF
            topology: triangle strips
```

Two vertex formats, strides 28 and 32; 179,322 vertices and 256,732 indices
across 1227 descriptors.

## Not established

- **What the 8 and 12 trailing bytes of each vertex format hold.** Unchanged
  from cycle 1425.
- Whether `0xFFFF` is retail's own restart convention or the hardware's. The
  Xenos supports a primitive reset index and `0xFFFF` is its usual value, but
  nothing in the corpus has been read setting it — so it is a fact about the
  data here, not yet about the code.
- What `sections.third` is. Zero in every container measured, and its header
  comment says it is zero when `[+0x1C]` is zero, so no container in this
  package uses it.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 52
tools/tests                           Ran 79 tests, OK
census                                1041/1041 records, 1227/1227 descriptors, 0 refused
arbitration                           vertices 1227/1227, indices 1227/1227
```

## Next

**Decode one descriptor into a `DecodedGeometry` and put it on screen.**
Everything it needs is now addressed: two stride cases, positions at a known
base, indices at a known base with a known sentinel, and a demo renderer that
already draws line segments from a basis and a position.

The first picture should be one container's wireframe, not the mission — a
single model drawn from bytes reached entirely through contracted resolution is
the result worth having, and the scene around it is still invented.
