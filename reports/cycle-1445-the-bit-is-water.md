# Cycle 1445 — the bit is water

## Qualification

- **No Ghidra run and no oracle pass.** The flat image, `analysis/class-map.tsv`
  and the extracted archive.
- No product C++ changed; ctest stays **53**. **No contract entry** — nothing is
  ported yet.
- New: `tools/ppc_read.py`, `tools/mission01_heightfield.py`,
  `tools/audit_map_water_bit.py`.

## An instrument, first

Every instruction this campaign has quoted came out of a Ghidra listing, and
`exports/82102568.json` is why that had to stop. It carries *"Control flow
encountered bad instruction data"* and **thirty-three removed blocks** — the C it
prints is a paraphrase of a read that never finished. `exports/` truncating
VMX128-heavy functions is a known shape; the decompiler quietly reconstructing
around what it could not decode is the same shape one level up.

`tools/ppc_read.py` decodes the bytes. It covers the integer, load/store and
floating subset a field read is made of and prints everything else as `.long`,
which is how a run of VMX128 announces itself instead of vanishing.

**Measured before use**, against the instructions cycle 1442 quoted:
`lfs f11,0(r4)` / `lfs f10,8(r4)` / `fadds` / `fmuls` / `fctiwz`, and
`srawi r11,r11,3` / `lbzx` / `srw` / `clrlwi r3,r11,31`. All match — and it shows
an **`addze r11,r11`** at `0x8210212C` that cycle 1442's four-line quote dropped,
so the bit index is a signed division by eight, not a shift.

## Where the class came from

`tools/find_materialised_address.py` on the vtable `0x8205C9A4`:

| | |
|---|---|
| `0x820FA258` | the constructor — writes the vtable, ~500 KB of fields |
| `0x820FA6F0` | the destructor — reverts to `0x8205C82C` |

`0x8205C82C` is **`ACE6::CAce6MapManager`** in the class map, and `CMapManager`'s
own RTTI chain names it as the base. Two readings agreeing.

And the constructor's callers name the owners:

```
0x823D19D0   a static instance at 0x8293BA90
0x82097388   CX360MissionManager<CAce6MissionManagerOnline>      x2
0x821A41D8   CX360MissionManager<CAce6MissionManagerCampaign>    x2
0x821A35E0   CX360MissionManager<CAce6MissionManagerReplay>      x2
```

**Two CMapManagers per mission manager.** The map is per-mission, not per-frame.

## The terrain heightfield

`0x82102568` reads `[this+0x0C]` and `[this+0x10]` — cycle 1444's other pair of
pointers — and its arithmetic is unambiguous once the bytes are decoded:

```
0x82102590  lwz    r6,0xC(r24)          the 16 x 16 byte grid
0x821025A0  srawi  r5,r9,4              coarse x, bounds-checked 0..15
0x821025F8  add    r21,r11,r5           index = coarse_z * 16 + coarse_x
0x8210261C  lwz    r8,0x10(r24)         the patch array
0x8210263C  lbzx   r10,r6,r21           patch id
0x82102648  mulli  r10,r10,0x4204       * 16900
0x82102700  rlwinm r8,r11,6,0,25        \  r11 = fine_z * 260
0x8210270C  add    r11,r8,r11           /
0x82102710  add    r11,r11,r9           + fine_x * 4
0x821027C0  addi   r10,r10,0x104        the sampler's row step
```

`0x104` is 260 bytes = **65 floats**, and `65 x 260 = 16900 = 0x4204`. So a patch
is **65 x 65 floats**: 16 cells of 4 samples plus the shared edge. One cell is
512 units (cycle 1442), so **one sample is 128 world units**, and 16 x 16 x 64 + 1
samples span exactly **131,072 units — the ±65,536 the `+65536.0` bias implies.**

The sampler takes the **maximum** over a 5 x 6 neighbourhood (`fcmpu`/`fmr`,
`f0 = max`) and bails if that maximum is below the `y` of both input points. Two
points, `x` at `+0`, `y` at `+4`, `z` at `+8`: **`0x82102568` is a segment-versus-
terrain test**, and the caller `0x82102E70` converts both endpoints to cells with
the same `(w + 65536) x 1/512`. `0x82069BC0` is `9990.0`, the absent-sample
sentinel.

## And the blobs, by arithmetic

`021_FHM/` holds four 16 x 16 byte grids of **the same map** with different
alphabets — MCA (19 ids), `004` (74), `009` (24), `006` (1) — each paired with an
array by a division that comes out whole:

| grid | ids | array | | |
|---|---:|---|---:|---|
| MCA | 19 | MCI | 9,728 = 19 x 512 | cycle 1443 |
| `009` | 24 | `010` | 12,288 = 24 x 512 | |
| `006` | 1 | `007` | 16,900, **all `0xFF`** | absent |
| **`004`** | **74** | **`005_Bl_02_b8`** | **1,250,604 = 74 x 16,900 + 4** | |

`005`'s first floats are 59.00, 67.03, 66.29, 65.29.

## The control, and it is the point

If the 65th row and column are the shared edge, a patch's column 64 must equal
its right neighbour's column 0.

```
shared edge along x:  15600 match,  0 mismatch,  worst |diff| 0.0000
shared edge along z:  15600 match,  0 mismatch,  worst |diff| 0.0000
CONTROL, random block pairs: 3736 match, 9264 mismatch
```

Exact agreement on 31,200 samples where agreement is not the default. Heights run
0.00 to 487.44 over 1,050,625 samples.

Rendered, it is **a coastline, a bay, and two rivers** —
`reports/mission01-terrain/mission01-terrain-relief.png`.

## Which names what cycle 1440 refused to name

Cycle 1440 read MCA's structure — a diagonal stepping one cell per row, a
vertical line down column 9, twelve unique cells where they meet, a uniform field
below — and wrote: *"I am not going to name them. Calling `01` the sea and the
diagonal a river is the move that cost three cycles at 1428–1438."*

That was right, and it is now settled by rendering rather than by guessing. `01`
**is** the sea, the diagonal **is** a river, the vertical **is** a river, and the
twelve unique cells **are** the harbour where they meet.

## What the bit means

Four cycles refused to name it. There was no control. There is one now: the
heightfield comes from **different fields**, **different blobs** and a
**different function** from the bit query, so the two can be compared and a wrong
reading of either shows up as noise.

`tools/audit_map_water_bit.py`, 1,048,576 samples on one lattice:

```
  land=0 bit=0 :    26064 ( 2.49%)
  land=0 bit=1 :   497482 (47.44%)
  land=1 bit=0 :   524105 (49.98%)
  land=1 bit=1 :      925 ( 0.09%)
bit set on water, clear on land: 97.43%
```

> **The bit is water.**

The residual is not noise, and its shape is the evidence. Of 26,964 disagreeing
samples, **26,064 — 96.7% — are flat ground at or below 0.5 with the bit clear**:
the city, the harbour and the river banks, exactly where a proxy that calls
everything at elevation zero "sea" must fail. Only **925 samples (0.09%)** carry
the bit over ground above 0.5, and they lie along the two rivers, which are
narrower than the 128-unit height lattice and which the 8-unit bit grid resolves
and the heightfield cannot.

The crude instrument in that comparison is my height proxy, not the bit.

## Not established

- **What the game uses the bit for** — rendering water, denying ground collision,
  or both. "It marks water" is what the data says; which subsystem consumes it
  still needs the caller, and slot 2 has none in the corpus.
- `008` (42,250 = 65 x 65 x 10, all zero), `010`, `011`, `012`, `013`.
- The 4 bytes past `005`'s 74th patch.
- `015_FHM.fhm` (15 MB) and `016_FHM.fhm` (140 MB), still unopened.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
contract_artifacts (three live)       pass  cited=123 match_head=123
contract_addresses (all)              pass  cited=301 supported=301
tools/tests                           Ran 79 tests, OK
```

**A standing defect, found by running the artefact checker over `*.json` rather
than over the live contracts.** `analysis/contracts/mission01-native-gate.json`
— the v1 contract, superseded by v2 and by `final-gate-v3` — cites three
`native-session.json` files under `headless-p3-runtime/`,
`headless-p4-retail-radio/` and `headless-v2/` that **`git ls-files` shows were
never committed**. So it has been failing `audit_ac6_contract_artifacts.py` for
as long as it has existed, and no gate noticed because the gate runs the live
contracts. Nothing this cycle touched it. Recorded rather than fixed: deleting a
superseded contract and re-pinning three absent captures are two different
decisions, and neither belongs in a cycle about terrain.

## Next

**Port the heightfield to the product and draw it.** The decode is four
arithmetic steps with a control that passes exactly, the world transform is
already contracted, and `demo_flight_view` already has `draw_mesh_lit` and a
depth buffer. A 1025 x 1025 field at 128 units is 2.1 M triangles whole, so the
patch grid is also the draw unit — which is what it is for.

That is the first Thread B behaviour that can carry `static` + `native-test` +
`derivation` evidence on the day it is written, and it puts ground under the
aircraft that has been flying over nothing since cycle 1417.
