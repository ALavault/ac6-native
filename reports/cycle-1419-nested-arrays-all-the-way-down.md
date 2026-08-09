# Cycle 1419 — nested arrays all the way down

## Qualification

- **No Ghidra run and no oracle pass.** The extracted asset tree and the
  product's own NDXR reader.
- No product C++ changed; ctest stays **51**. **No contract entry** —
  reconnaissance, and the second Thread B cycle.
- `tools/mdlp_index.py` gains FHM parsing and `--resolve`;
  `tools/ndxr_open_probe.cpp` now resolves by index instead of scanning.

## The second level has the same shape as the first

Cycle 1418 found `MDLP` to be an array. The FHM inside each entry is another
one:

```
+0x00  "FHM "
+0x10  sub-entry count
+0x14  `count` big-endian offsets, relative to the FHM's own base
```

So the whole resolution is nested arrays and **nothing has to be searched for**:

```
MDLP[i]  ->  FHM  ->  FHM[j]  ->  { NDXR | NTXR | MATE | a nested FHM }
```

Checked across the whole 29 MB file rather than on the entry I read:

| | |
|---|---:|
| FHMs that parse | **94 of 94** |
| sub-entries | 1,480 |
| tables not ascending | 0 |
| anomalies | 0 |
| **NDXR occurrences at a tabulated offset** | **292 of 292** |

That last row is the one that matters: cycle 1418 located its 292 containers by
scanning for the magic, which is a search. Every one of them is at an index.

## What the 1,480 sub-entries are

| magic | count |
|---|---:|
| `MATE` | 381 |
| `NDXR` | 292 |
| `NTXR` | 86 |
| `FHM ` | 47 |

Materials are in the package, indexed the same way — which is the other half of
gap #8. And 47 nested FHMs, matching cycle 1418's count of entries carrying
geometry, though what that nesting means is unread and not claimed here.

## A "disagreement" that was not one

Comparing the FHM table's implied span against each container's own declared
length at `+0x04` reported **292 disagreeing, 0 agreeing**, which reads as a
contradiction between two indices.

It is not. The table's span is the **larger** in all 292 cases, and **every byte
of the difference is zero** — 10 bytes for 199 of them, 7 for 54, and the
entry's own tail padding for the last sub-entry of each FHM.

One is a padded span, the other a content length. Reporting "292 disagreeing"
would have been true of the numbers and false about the file, and it is exactly
the kind of statement that sends a cycle looking for a defect that does not
exist. The tool prints the padding check rather than the raw comparison.

## The chain, end to end, with the reader on the end of it

`tools/ndxr_open_probe.cpp` now walks `MDLP[i] → FHM[j]`, trims each NDXR span
to its declared length, and hands it to the product's `NdxrContainer::Open`:

```
tried=292  opened=292
```

**292 of 292**, with no magic scanned at any level. That is the packaging layer
cycle 1418 said was the missing piece, established with a control at every hop:
the entry table (94/94 `FHM `), the sub-entry table (292/292 tabulated), the
length relationship (292/292 zero padding), and the reader (292/292 opened).

## Not established

- **What the 47 nested FHMs are for.** They are counted, not read.
- The two unnamed word patterns that appear beside every container —
  `00000091` exactly 292 times and `00000001` 288 times. The first matching the
  NDXR count exactly is suggestive and suggestive is not established.
- **Which id Mission 01 asks for.** Still the open one, and still the step that
  makes this geometry *this mission's* rather than *some* geometry.
- The even/odd pairing killed in cycle 1418 remains dead.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
mdlp_index --resolve                  1480 sub-entries, 292 NDXR by index
ndxr_open_probe                       292/292 opened, nothing scanned
```

## Next

**Join a Mission 01 model reference to an MDLP index.** Everything below that is
now an index lookup with a control on it; what is missing is the top of the
chain — the integer the mission data carries, and where it is read.

`analysis/` already holds the mission's object records from Thread A's earlier
work, and `retail_mission_state.cpp:311` is the line the plan names as throwing
the model index away (`unit.asset = object.category` discards `model_bindings`).
That line is where the two halves meet, and it is a read before it is an edit.
