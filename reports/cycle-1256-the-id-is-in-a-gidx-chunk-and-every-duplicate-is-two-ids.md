# Cycle 1256 — the id is in a GIDX chunk, and every duplicate is one of two ids

## Qualification

Ghidra project `ghidra-projects-xenon/ac6-xenon` for instructions. `default.xex`
SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
Corpus: `reports/logs/cycle-739-pac-mission-gate/fhm/**/*.ntxr`, `runtime_idx_*`
excluded — **346 wrappers**. Retail bytes stay local and are not committed.
**No oracle pass was spent.**

## What cycle 1255 said it needed, and what it actually needed

Cycle 1255 stopped on the id extraction and named the next step: *derive the
per-entry record layout from the NTXR reader `0x8234B300`*. That step was wrong
in a useful way, and the correction is the point of this cycle.

`0x8234B300` is **not** a record walker. It is a nine-instruction validator:

```
8234b304  lis   r9,0x4e54
8234b30c  ori   r9,r9,0x5852     ; 'NTXR'
8234b310  lwz   r11,0x0(r10)
8234b314  cmplw cr6,r11,r9
8234b318  bnelr cr6
8234b31c  lbz   r11,0x4(r10)     ; version
8234b320  cmpwi cr6,r11,0x1
8234b324  beq   cr6,0x8234b348
8234b328  cmpwi cr6,r11,0x2
8234b32c  bnelr cr6
8234b330  lbz   r11,0x5(r10)     ; sub-version: 0 or 0x0C for version 2
```

So the header is magic, a version byte at `+0x04`, a sub-version at `+0x05`, and
the entry count as a `u16` at `+0x06`. The corpus reads `02 0c 00 01` — version
2, sub `0x0C`, one entry — and the calibration wrapper reads `02 0c 00 06`.

The field decoder is the next function, `0x8234B360`: `lbz r11,0x13(r3)` format,
`lhz r11,0x14(r3)` width, `lhz r11,0x16(r3)` height, with a 47-entry table at
`0x826767C0` indexed `×8`.

**The id is in none of those.** The file is a sequence of tagged chunks, and the
id lives in a `GIDX` chunk — which is exactly the established registry key,
`GIDX+0x08`, already known and already the thing the map is keyed on. The
locator is the four-byte tag, not an offset.

### A fixed offset derived from one file is not a layout

I first anchored on the calibration wrapper and got a clean-looking answer:
records at `0x10`, stride `0x50`, id at `+0x48`. Every one of its six entries
agreed, and the six ids came out consecutive — `0x10002215` … `0x1000221A`.

Applied to the corpus it produced **336 distinct file contents carrying 33
distinct ids**, which is impossible: different textures do not share an id. The
value at `+0x58` in a one-entry wrapper is the ASCII `GIDX` tag itself, and the
"ids" I had counted were tag bytes and chunk sizes. The earlier heuristic — the
first word at or above `0x10000000` — had failed differently, returning five
offsets. **Two wrong instruments, and each looked right on the file it was built
from.**

## Established

**Locate the `GIDX` tag, require its size word `+0x04` to be `0x10`, read the id
at `+0x08`.**

Controls, each able to fail:

- **Structural.** The number of `GIDX` chunks equals the header's declared entry
  count in **346 of 346** wrappers, zero mismatches.
- **Named calibration.** `exports.pre-s0/first-linked-0x10002215.ntxr` declares
  six entries; the locator returns `0x10002215, 16, 17, 18, 19, 1A` — the first
  being the id in the file's own name.
- **Prior count.** The corpus yields **205 distinct ids**, which is the number
  cycle 1248 reported from a script that was not preserved.
- **Prior split.** 192 of the 205 fall below `0x10000000`, which is cycle 1209's
  count exactly.

## The answer cycle 1255 left open

**Only two ids are ever duplicated, and they account for all of it.**

| id form | distinct ids | entries | ids appearing more than once |
|---|---:|---:|---:|
| `>= 0x10000000` — never biased at mount | 13 | 13 | **0** |
| `< 0x10000000` — biased at mount | 192 | 333 | **2** |

346 entries over 205 ids is 141 extra copies, and `0x08000000` (115 entries) plus
`0x0F000000` (28 entries) account for 141 of them exactly. **Every other id in
the corpus is unique**, so for 203 of 205 ids the first-wins policy derived in
cycle 1255 never fires and mount order is not load-bearing.

The two that do repeat are round numbers, both below the threshold, and their
copies differ in content. The low ids also run in consecutive blocks —
`0x1049, 0x104a, 0x104b, …` — which is the shape prior sessions recorded for the
`mode = 1` packs as *base + index within the pack*.

**So the duplicate-mount question and the `mode = 1` question are the same
question**, and this measurement says which: the collisions are confined to the
biased id space, where the bias is the mechanism meant to separate packs, and
where Mission 01's bias is zero.

## Not established

- **Whether `0x08000000` and `0x0F000000` are ids at all.** They may be bases to
  which an entry ordinal is added at mount, which is precisely the open
  `mode = 1` rule. Nothing here reads that arm; the investigation into it was cut
  short by a session limit and is unfinished.
- **Whether the 346 wrappers of this extraction root are the packs Mission 01
  mounts.** The tree holds 1,053 `.ntxr` files but only 337 distinct contents:
  the same packs re-extracted under `cycle-738`, `cycle-739` and `runtime_idx_*`.
  One root is the right denominator for a per-pack question, and which root
  corresponds to the live mount set is not something this cycle read.

## Corrections

- **Cycle 1248's "847 duplicates over 205 ids across 1052 packs".** The 205 is
  right and reproduces. The 1052 counted **extraction replicas as packs** — the
  same wrapper extracted three times under different roots — which inflates the
  duplicate count from 141 to 847. Duplication of the extraction is not
  duplication of the mount.
- **Cycle 1255's named next step** — "derive the per-entry record layout from
  `0x8234B300`" — pointed at a validator. The layout question was the wrong
  question; the id was never in the entry record.
- **This cycle, twice, against itself.** The `+0x48` layout and the
  `>= 0x10000000` heuristic both produced confident numbers from instruments
  built on a single file. The structural control — GIDX count against declared
  count — is what separated them, and it was available before either.
