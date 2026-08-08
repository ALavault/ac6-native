# Cycle 1203 — the product and the derivation read different fields, and neither stride is established

Cycle 1189 flagged `src/native_geometry_raster.cpp` as a **measured, unaudited**
NDXR reader citing zero retail addresses. Cycles 1194–1202 built the derivation.
This cycle sets them side by side, which is the whole point of having built it.

## Where they agree, and it is most of the header

| the product reads | the derivation says | agree |
|---|---|---|
| `be32(0x04) == filesize` | `[+0x04]` is the file's own size (537/537) | ✔ |
| `be16(0x0A)` object count | `u16 [+0x0A]` record count, `0x823556E0` | ✔ |
| `be32(0x10/0x14/0x18/0x1C)` four sizes | four section extents, `0x82350F08` | ✔ |
| object table at `0x30`, stride `0x30` | records at `file+0x30`, stride `0x30`, `0x823555D0` | ✔ |
| `+0x2A` per-object polygon count | `rec+0x2A` sub-count | ✔ |
| `polygon_base = 0x30 + [+0x10]` | body base `buf + [buf+0x10] + 0x30` | ✔ |

Six independent agreements, and the last one is the constant cycle 1196 had to
build a special control for. **The product's header reading is sound**, and it was
arrived at without any of these addresses.

One difference that is not a conflict: the product places the polygon descriptors
immediately after the object table, while retail follows the pointer at
`rec+0x2C`. On this corpus they coincide — for the sampled file the object table
ends at `0x300` and `rec[0]+0x2C` is `0x300`. The product hardcodes what retail
dereferences.

## Where they conflict, and it is the descriptor

The product reads a polygon descriptor as:

```
+0x00 index_offset   +0x04 vertex_offset   +0x0C vertex_count
+0x0E format (u16)   +0x20 index_count
```

The derived retail path (`0x82355468`) reads `+0x0C`, `+0x0E`, `+0x10`, `+0x14`,
`+0x18`, `+0x1C`, `+0x22`, and writes `+0x1C` and `+0x28`.

**No instruction on the derived path reads `+0x00`, `+0x04` or `+0x20`.** And the
product reads none of the four stream pointers at `+0x10..+0x1C` that retail
relocates and resolves. The two readings overlap on exactly two fields, `+0x0C`
and `+0x0E`, and disagree about everything else in the record.

## The stride, and I cannot settle it either way

The format field census, over 13,014 descriptors:

| `u16` at `+0x0E` | count | product's stride |
|---|---|---|
| `0x0613` | 12,978 | 32 |
| `0x0611` | 36 | 28 |
| `0x0711`, `0x0721` | **0** | 44, 52 — never exercised |

Retail reads only the **high byte** (`lbz r10,0xe`), which is `0x06` for both, and
indexes `0x82012C40` at `(code & 0xF) * 4 = 0x18` → **`0x14`, twenty bytes**.

The product's own validity rule — `vertex_offset % stride == 0` — looks like the
discriminating control cycle 1198 wanted. It is not:

| stride | `0x0613` (12,978) | `0x0611` (36) |
|---|---|---|
| 16 | **100.0%** | 66.7% |
| 20 (derived) | 94.3% | 25.0% |
| 24 | 36.4% | **100.0%** |
| 28 (product) | 16.8% | **100.0%** |
| 32 (product) | **100.0%** | 25.0% |

The product's constants pass at 100% for their own format — and so does 16 for
`0x0613`, and so does 24 for `0x0611`. **The offsets are simply 32-aligned**, so
every divisor of the alignment scores perfectly. The test cannot separate 16 from
32, and with 36 samples it cannot separate 24 from 28. It is the cycle-1196 shape
a third time: a check that cannot fail against the rivals that matter.

What it does show is that **20 does not divide `+0x04`**. That refutes my stride
only under the product's premise that `+0x04` is a vertex offset in stride units
— and `+0x04` is a field **retail never reads**. So it is evidence against a
claim neither side has established.

**Conclusion: the stride is unestablished, on both sides.** Cycle 1198 recorded
`0x14` as derived-but-uncontrolled; it stays that way, and the product's 28/32
are now equally exposed as uncontrolled. Half of the product's format table
(`0x0711`, `0x0721`) is never exercised by any file in the corpus.

## Correcting myself within this cycle

My first run of this test aggregated both formats and reported `% 28` at 17.0%,
which reads as a refutation of the product. It is not — the product assigns 28 to
`0x0611` only, and restricted to those 36 descriptors it is 100%. I conflated the
partition with the population. The corrected table is the one above; the
aggregate one was wrong for four minutes.

## What this means for the port

The header can be ported now, with citations, and it will not change behaviour —
the product already computes the same six things. **The descriptor cannot**, and
that is where 2g actually stands. Retail resolves geometry through the id
registries of cycles 1200–1202; the product reads offsets out of the same bytes
and gets a picture. Both cannot be describing the same format correctly, and the
question is not settled by anything I have run.

## Not established, stated plainly

- Which reading of `+0x00`/`+0x04` is right, or whether the product's picture is
  correct for a reason its field names do not capture.
- The vertex stride, on either side.
- Whether `0x0711`/`0x0721` exist anywhere outside Mission 01's corpus.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
13,014 descriptors, per-format
```

No product code changed. **In particular the reader was not "fixed" to match the
derivation** — that would have replaced a measured guess with an uncontrolled one.
