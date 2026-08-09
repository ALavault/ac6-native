# Cycle 1418 — the format is already an array

## Qualification

- **No Ghidra run and no oracle pass.** The extracted asset tree, the product's
  own NDXR reader, and two new probes.
- No product C++ changed; ctest stays **51**. **No contract entry** — this is
  reconnaissance, and it is the first Thread B cycle of the campaign.
- New: `tools/mdlp_index.py`, `tools/ndxr_open_probe.cpp`.

## The decision this was supposed to gate

The plan's first Thread B decision: **how a model is loaded**. Cycle 1246
established that retail resolves assets by **integer id** through registries and
never walks the directory — so porting an FHM directory walk would be porting
something the game does not do, and an offline extraction is a manifest under
another name, which JF exists to eliminate.

**The decision largely dissolves, because the file format is already an array.**

## `001_MDLP.mdlp` is an integer-indexed array

```
+0x00  "MDLP"
+0x04  94                    entry count
+0x08  29,097,984            the file's byte length, EXACTLY
+0x0C  0x1000                offset of the entry table
+0x10  0x2000                offset of the data
```

Entry `i` sits at `data + table[i]`. Checked rather than assumed:

- **all 94 entries resolve to an `FHM ` magic** — 94 of 94, no exceptions;
- the offsets ascend;
- the declared size matches the file exactly.

`MDLP[id]` is an array index. There is no walk to port and no manifest to fall
back on, which is the answer the decision was waiting for.

## Where the geometry actually is

A first scan for `NDXR` **as a file magic** across all 1,111 extracted files
found **zero**, and the tree has no `.ndxr` files at all. Scanning file
*contents* instead:

| | |
|---|---:|
| NDXR occurrences | **292**, every one inside `001_MDLP.mdlp` |
| NTXR occurrences | 184 across the FHM files, 86 inside MDLP |

So the mission's geometry is not missing and it is not loose on disk — it is
packaged, 292 containers inside one 29 MB array. Had I stopped at the
file-magic scan I would have reported the geometry absent.

## A plausible rule, killed by its own exception check

The NDXR-bearing entries looked strictly even — 0, 2, 4, 6, 8, 10, … — which
reads immediately as **47 pairs of {geometry, textures}**, and would have
resolved gap #8 (materials → textures) as a bonus. It is a good-looking rule and
I was about to write it down.

Counting the exceptions instead of admiring the pattern: **45 of the 94 entries
break it.** Most even entries also carry an NTXR; entries 18, 19, 43 and 45 carry
NDXR on the odd side; 87 and 89 are empty 4,096-byte stubs.

The pairing is **not established** and neither tool claims it. This is the
standard cycles 1111 and 1113 set, applied to my own finding twenty minutes old.

## The reader works on every real container

The cycle's stated question was whether the first port is a *reader* or an
*accessor* — gap #3 says `NdxrContainer` "serves no bytes",
`bytes_`/`size_` private with no accessor.

It is neither. `tools/ndxr_open_probe.cpp` runs the product's own
`NdxrContainer::Open` over all 292 embedded containers:

```
first attempt:   tried=292  opened=0    size-mismatch 292
```

Every single one refused. But the refusal is the port's own guard, and the
source says so in as many words:

```cpp
// [+0x04] is the file's own byte length. Retail does not check this; it is
// free here and it is the cheapest possible guard on a truncated read.
if (Be32(base + 0x04) != remaining) { ... kSizeMismatch ... }
```

`Open` requires the buffer to be **exactly** the container's declared length. An
embedded container handed "from its magic to the end of its MDLP entry" is a
buffer that is too long. Trimming the span to the declared length at `+0x04`:

```
second attempt:  tried=292  opened=292  OPENED 292
```

**292 of 292.** The decoder is not the gap. What was missing is a packaging
layer — `MDLP[id] → FHM → an NDXR span trimmed to its own declared length` —
and nothing in the product does that last step.

Worth separating clearly: the first result would have read as "the decoder fails
on real data, 292 refusals." It was my span that was wrong, not the reader, and
one look at the refusal's own source said which.

## On the assets living in a logs directory

The plan lists "give them a stable input path" as an infrastructure
prerequisite. Two facts change how urgent that is:

- the tree is **not tracked by git** — but neither is `analysis-input/`, which
  holds the XEX every contract is qualified against. Large retail-derived inputs
  living outside the repository is the campaign's existing convention, not an
  anomaly this thread introduces;
- so the real defect is only that they sit under `reports/logs/`, a path whose
  name says "log" and whose sibling directories are disposable.

## Not established

- The FHM layout inside an entry. `MDLP[id]` gives an FHM; how an FHM is indexed
  to reach a particular NDXR is unread, and 292 containers across 47 entries
  means an entry holds several.
- Which id Mission 01 asks for. The mission data's model references have not
  been joined to these indices.
- The even/odd pairing, above.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
mdlp_index                            94/94 entries FHM, size exact
ndxr_open_probe                       292/292 opened
```

## Next

**The FHM layout inside an entry**, because it is the one hop between
`MDLP[id]` and a container the reader already accepts. It is a bounded read of a
header whose magic the tree carries 133 times, and it turns the probe's
byte-scan for `NDXR` — which is a search, not a resolution — into an index
lookup like the one above it.

Then joining a Mission 01 model reference to an MDLP index, which is the step
that makes the geometry *this mission's* rather than *some* geometry.
