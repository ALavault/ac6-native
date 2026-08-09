# Cycle 1423 — the last hop

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- ctest 51 → **52**. Contract 29 → **30 behaviours** — the first Thread B entry.
- New: `reconstruction/ace-combat-6/{include/ac6,src,tests}/retail_container_index.*`,
  `tools/audit_container_index_microexec.py`,
  `analysis/assets/container-index-microexec.tsv`.
  `tools/ndxr_open_probe.cpp` rewritten to call the ports.

## The chain closes

```
directory: 94 entries, all FHM = yes
NDXR reached by index: 292
  array1 length == the container's own +0x04: 292
  opened with that length: 292
```

Every hop through **ported code and nothing else**: `ModelDirectory` for the
MDLP level, `ContainerIndex` for the FHM level, `NdxrContainer::Open` on the
span they produce, with the length read from array 1. No magic is scanned for at
any level and no length is computed by subtraction.

Cycle 1418's probe scanned 29 MB for a four-byte string. Cycle 1419's walked the
tables by hand. This one asks the product.

## What was ported

`0x82234C18`, 109 instructions, and `0x82234DD0`, 13 — both leaves, no calls, no
vector. The header they read is general rather than FHM-specific:

```
+0x04  version
+0x05  ENDIAN FLAG        not 1 -> every field below is byte-swapped
+0x06  header size, u16
at file + header_size:  count, then FOUR parallel arrays of that many dwords
```

and the descriptor is written field by field, which is why the differential
compares its nine words rather than a return code. `+0x14` is deliberately
absent from the port: the parser never writes it, which is why its caller zeroes
the whole struct first.

## The one thing a tidy port gets wrong

On the byte-swapped path the three array pointers are computed from the **raw,
unswapped** count. `0x82234C7C` multiplies the word as loaded, `0x82234C84`
onward store the four pointers, and only afterwards does `0x82234CD0` replace
the count at `+0x00` with the swapped one. So a byte-swapped container gets array
pointers derived from a count with its bytes reversed, running far out of range.

My first port swapped the count before computing the arrays — the sensible
reading, and wrong. The differential rejected it at exactly three fields:

```
FAIL parse-byte-swapped array1: retail=0xf8000014 port=0xb000005c
FAIL parse-byte-swapped array2: retail=0x40000014 port=0xb00000a4
FAIL parse-byte-swapped array3: retail=0x88000014 port=0xb00000ec
```

**No shipped file reaches that path.** All 94 FHMs carry flag 1. A port checked
only against real data would have passed every case and carried a silent
divergence for any file that did not — and the case that caught it is synthetic,
built because cycle 1422 read the parser rather than the file.

That is the argument for reading code you have working data for: the data
exercises the path it exercises, and nothing else.

## A control that encodes cycle 1419's mistake

Cycle 1419 computed each sub-entry's length by subtracting neighbouring offsets
and reported 292 disagreements with the containers' own declared sizes. Array 1
is the length and agrees at 292 of 292.

The test asserts that subtraction **must disagree** with array 1 on a fixture
where the padding is non-zero. A port that quietly went back to subtracting
would pass every other case here and fail that one.

## Not established

- The in-place byte-swap rewrite of the four arrays
  (`0x82234CA0`..`0x82234DC0`) is read and **not modelled**: this port takes an
  immutable buffer, and the descriptor it produces — which is what the
  differential compares — is unaffected. Stated in the header rather than left
  to be discovered.
- Arrays 2 and 3, zero in every FHM measured. Their being zero is not evidence
  that they are unused.
- Slot 7, `0x821C1960`, and the constant `152` that travels to both slots.
- What `0x821C17B0`'s comparison of a byte against 192 selects.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 52
tools/tests                           Ran 79 tests, OK
audit_container_index --check         8 cases, 32 values, 0 failures
ndxr_open_probe                       292/292 by index, 292/292 opened
```

## Next

**Geometry out of a container.** `NdxrContainer` opens all 292 and exposes
`Record`, `Material` and `TextureRef`; what nothing does is turn those into the
vertices `draw_world_geometry` wants. The plan's gaps 4, 5 and 6 are all on that
side — the decoder locked to the manifest, the model index not reaching the
world, and `frame.mission_ready` false on the retail path by design.

Gap 6 is the one to take first, because it is a decision rather than code: a
retail-path drawing entry point, not synthesised contract records.
