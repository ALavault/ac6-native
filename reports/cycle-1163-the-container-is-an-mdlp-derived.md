# Cycle 1163 — the container is an MDLP, derived; and 494 world textures decode

## The provenance gap closes

Cycles 1158 and 1159 established that `0x8228E9B8`'s arithmetic *is* the MDLP's
layout, and refused to merge that with "this container is that file". The
provider is now read, and they are the same thing.

`0x820A7070`'s first argument is a **`CX360UnitManager`** — two instances
embedded in `CX360MissionManager<ACE6::CAce6MissionManagerReplay>` at
`this+0x12B440` and `this+0x12B85C`, their vptr `0x82055190` installed by the
constructor at `0x821A36D4`/`0x821A36E0` and confirmed independently by the
destructor. Both names come from J2's class map.

Its vtable slot `+0x0C` is `0x820A85E0`, and the container writer it ends in is
`0x8228E988`:

```
8228e988  stw r4,0x0(r3)      ; word0 = the blob
8228e98c  lwz r11,0xc(r4)
8228e990  add r11,r11,r4
8228e994  stw r11,0x4(r3)     ; word1 = blob + *(blob+0x0C)
8228e998  lwz r11,0x10(r4)
8228e99c  add r11,r11,r4
8228e9a0  stw r11,0x8(r3)     ; word2 = blob + *(blob+0x10)
```

with the count read as `*(word0 + 0x04)`. Set that against the MDLP header
cycle 1157 validated on Mission 01's own file:

| the writer uses | the MDLP has |
|---|---|
| `*(blob+0x04)` as the count | `+0x04` = 94 |
| `blob + *(blob+0x0C)` as the offset table | `+0x0C` = `0x1000`, a 94-dword table |
| `blob + *(blob+0x10)` as the entry base | `+0x10` = `0x2000`, `base+table[i]` on `FHM ` 94/94 |

Three fields, three matches, from code rather than from resemblance. **The model
container is an MDLP blob**, and bytes `+0x61`/`+0x62` are indices into it.

## Where the blob comes from

`0x820A85E0` does not open a file. It re-derives the node the loader already
holds — `0x820A8600`–`0x820A8614` is byte-for-byte the loader's own prologue at
`0x8219BE64`–`0x8219BE78`: same root `0x829E6218`, same key `*(0x8293BA10)+0x54`,
same lookup `0x821D2FC0`. It then descends one level by a hashed name built with
`sprintf` from the format string at `0x82067B00`, `"DPL::[%#x,%#x]"`, and takes
**chunk index 1** of that sub-node's directory at `+0x20`.

So the MDLP is payload-resident: word0 points *into* the mission's own loaded
data, which is why cycle 1157 found the file sitting beside the scenario
container.

Not established, and left that way: which `<id>` the mode tables at `0x82065840`
/ `0x82065880` / `0x820658B8` yield for Mission 01. It depends on two runtime
values and is not a static constant.

## The texture packs, unpacked

Separately, and measured rather than derived: an NTXR inside the MDLP is a pack.
Header word 1's low half is the texture count, and after the first descriptor sit
that many **0x50-byte records** — `eXt`, then `GIDX` with a sequential id at
`+0x18`, then a full descriptor at `+0x20` whose data offset is relative to that
descriptor's own base.

```
records found                              522   (= the GIDX census exactly)
with correct eXt+GIDX signatures           518
rebuilt as single-texture wrappers         518
decoded by the unmodified product decoder  494
```

The 494 are aircraft and vehicle atlases at 2048×2048 and 1024×1024 — panel
lines, hatches, greebles — plus the building facades cycle 1162 already showed.

**The product decoder was not changed to achieve this.** The packs were split by
a diagnostic script into per-record wrappers and `ac6-ntxr-extract` was pointed
at them unchanged, which is the honest arrangement while the pack directory is
only measured: 522 records matching a census taken by an independent method, and
518 carrying both signatures, is evidence about a layout, not a derivation of it.

The first attempt at that split was wrong in a way worth recording: it gave every
record the rest of the file as its payload, so only the last texture of each pack
had a payload matching its surface. 82 decoded, 436 refused. The size check
caught the bug in the tooling exactly as it had caught the packs themselves.

## What would make the pack layout derivable

The retail reader for the `eXt`/`GIDX` record chain. `0x8234B268` already walks
a per-level array to find source offsets; the pack directory is the neighbouring
question and has not been read.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed. Decoded pixels stay local.
