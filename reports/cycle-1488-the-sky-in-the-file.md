# Cycle 1488 — the sky in the file

## Qualification

- **No Ghidra run and no oracle pass.** The archive, and the sweep's loader
  derivation re-used with its addresses.
- Product C++ unchanged; ctest stays **60**. Tool change only. **No contract
  entry** — the row orientation and column averaging are mine, named below.
- New: `tools/mission01_sph_dump.py`.

## Opening the .sph

The sweep established (cycle 1482, verified addresses): the loader reads
`sph%s.sph` into an inline buffer at `CMapManager+0x6850`, byte-reverses a
hard-coded **88 words** at `0x820FC960..0x820FC998` — so the file is exactly
**352 bytes, little-endian, two records of 176** — and Mission 01's file is
`022_FHM/002_00_00pF.bin`, the only 352-byte candidate.

Read as the loader stores it:

```
+0x00  f32 x2      15360 / 25600        (record 1: 7850.57 / 24085.6)
+0x10  u8[4] x12   twelve RGB0 colours, 3 rows x 4 columns    palette A
+0x40  u8[4] x12   twelve more                                palette B
+0x78  u8[4] x2    two dark colours (27,32,39) (32,36,43)
+0x80  f32 x12     0, 0, 0.57988, 0, then eight small weights
```

The `~1e-38` denormals that appear when the colour block is read as floats are
the tell: **packed bytes, not floats**, the same signature that separated MCA's
bytes from a float field in cycle 1440.

**Record 0 is a grey-blue haze palette; record 1 is saturated blue.** Two
records, and the mapset XML carries `sky1`/`sky2` as the same 127 keys with two
value sets — the two-state split appears in both files independently.

## What is retail's and what is mine

Retail's: all 24 colours per record, their 3 x 4 arrangement, the two head
floats, the tail weights. Mine: that **row 0 is the horizon and row 2 the
zenith** (rows run bright to dark, and a haze-bright horizon is the natural
reading — the `CSkySphere` consumer has not been read), and **averaging the four
columns**, whose meaning (azimuth?) is not established.

The scene renderer's sky gradient now interpolates those three row means —
`(126,146,169)`, `(105,128,152)`, `(86,106,126)` — instead of an invented blue.
One constant was written as `(127,147,169)` from eyeballing and corrected to the
computed mean before landing; the dump tool prints the exact rows so the next
reader does not transcribe.

`mission01-sph-sky.png`: the bridge under Mission 01's own overcast.

## Not established

- The head floats (radii? near/far?), the tail weights, palette B's role, the
  two extra dark colours.
- The row orientation and column semantics, as above.
- Which record a mission selects, and when.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**The textured flight video**, rendering in the background as this lands — the
full contracted chain over the atlas ground, which is the demo the reviewer's
three complaints have been building toward.
