# Cycle 1422 — a named singleton, and a general header

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus, the image's
  `.data`, and the extracted asset tree.
- No product C++ changed; ctest stays **51**. **No contract entry** — this cycle
  finds the derivation the next one will port against.

## The object, and why nothing wrote it

Cycle 1421 stopped at `[r30+1832]`, having found no store to it anywhere.

`r30` is `lis r30,-32150` = `0x826A0000`, so the field is the **global at
`0x826A0728`** — and the image holds a value there already:

```
static bytes at 0x826A0728: 826a0708 ...
```

A **statically initialised pointer** to `0x826A0708`, resolved at link time. That
is why no `stw` exists, and why cycle 1421's scan for one was answering a
question the file had already answered.

The object's first word is its vtable, `0x820674D8`, whose COL at `vtable-4` is
non-zero. Following it to the TypeDescriptor:

> **`CX360ActorModelSetup`**

A named class — the second the campaign has found by this route, after
`CX360UnitManager` at cycle 1385.

Its slots 6 and 7, the two that consume an FHM entry pointer, are
`0x821C1748` and `0x821C1960`.

## The FHM level is a retail code path after all

Cycle 1420 raised a real doubt: the FHM sub-entry table had a **file-derived**
reading and no retail function behind it, so it could not be contracted, and a
search for the `FHM ` magic across the corpus returned zero.

`0x821C1748` settles it in eight instructions:

```
0x821C1760  addi r3,r1,128       a descriptor on the stack
0x821C1798  bl   0x82234C18      built from r4, the FHM pointer
0x821C17A4  bl   0x82234DD0      indexed -- the same generic getter
```

**`0x82234C18` is retail's container-header parser** and `0x82234DD0` its getter.
The FHM level is retail's code, not merely retail's file layout, and the decision
cycle 1420 left open — port inside or outside the contract — resolves to
**inside**.

## What the header actually is, which is more than I had

```
+0x04  version byte
+0x05  ENDIAN FLAG          when it is not 1, every field below is byte-swapped
+0x06  header size, u16     byte-swapped first if the flag says so
at file + header_size:
       count, u32
       then FOUR parallel arrays of `count` dwords each
```

`0x82234C18` stores them at descriptor `+12`, `+16`, `+24`, `+28`, and when the
endian flag is not 1 it walks all four swapping every entry in place.

Two things there that a file-only reading could not have produced:

- **the endianness flag**. I read the FHM as big-endian because it looked
  big-endian. Every file in this archive happens to carry `1`, so the assumption
  held — but it held by luck, and the parser says the format does not require it.
- **four arrays, not one.** Cycle 1419 reported "a table of offsets" and that is
  only the first.

Checked against all 94 FHMs: **version 1, endian flag 1, header size `0x10`** in
every one. So cycle 1419's file-derived "count at `+0x10`, offsets from `+0x14`"
was right, and is now *explained*: `0x10` is the `+0x06` field, not a constant.

## And it corrects my own cycle 1419

Cycle 1419 compared the FHM table's implied span — next offset minus this one —
against each NDXR's declared length at `+0x04`, got **292 disagreeing**, and
resolved it as "one is a padded span, the other a content length."

That reading was right about the bytes and wrong about the format. **Array 1 is
the length**, and it equals each container's own declared size at **292 of 292**:

```
array0 (offsets)  0x140, 0x580, 0x1bf0, ...
array1 (lengths)  0x440, 0x1670, 0x198, ...      0x140 + 0x440 = 0x580
```

Retail carries the exact length in the header and I had computed a padded
approximation of it from the neighbouring offset. The port should read array 1,
not subtract offsets.

## The chain, now complete and all of it retail-derived

```
binding.primary  ->  ModelDirectory.entry(id)        0x8228E9B8   PORTED
   -> an FHM span
      -> 0x82234C18 parses its header                             the gap
         -> 0x82234DD0 indexes sub-entry j                        the gap
            -> array1[j] gives the exact length
               -> NdxrContainer::Open                             PORTED, 292/292
```

## Not established

- Slot 7, `0x821C1960`, read only as far as being the second consumer.
- What the constant `152` at every call site is.
- Arrays 2 and 3 — zero in every FHM measured, so their meaning is unread and
  their being zero is not evidence that they are unused.
- What `0x821C17B0`'s comparison of a byte against `192` selects.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
```

## Next

**Port `0x82234C18` and `0x82234DD0`** as a container reader, with the endian
flag honoured rather than assumed and the length taken from array 1. Both are
small — 109 and 13 instructions, no calls, no vector — and both are
differentiable by micro-execution the same way every flight behaviour was: seed
a region with a real FHM header, run, compare the descriptor's six words.

That closes the last hop, and the first NDXR reached entirely through retail's
own resolution becomes a contract entry rather than a probe.
