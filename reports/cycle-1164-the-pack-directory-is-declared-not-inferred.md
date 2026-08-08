# Cycle 1164 — the pack directory is a declared field, and retail's own writer says so

## The writer, which is better evidence than a reader

Looking for the code that *reads* the `eXt`/`GIDX` chain found the code that
**writes** it. `0x821D9478` constructs an NTXR wrapper in memory and lays the
whole structure out with immediates:

```
821d94f8  sth  r31,0x24(r3)     ; +0x24 = width
821d9500  sth  r30,0x26(r3)     ; +0x26 = height
821d94fc  li   r10,0x50
821d9518  sth  r10,0x1c(r3)     ; +0x1C = 0x50   the record stride
821d9510  li   r9,0xff0
821d9550  stw  r9,0x30(r3)      ; +0x30 = 0xFF0  the data offset
821d9528  ori  r5,r10,0x7400    ; 'eXt\0'
821d9554  stw  r5,0x40(r3)      ; +0x40 = 'eXt'
821d9558  stw  r8,0x44(r3)      ; +0x44 = 0x20   the eXt chunk size
821d955c  stw  r10,0x48(r3)     ; +0x48 = 0x10
821d9538  ori  r7,r7,0x4458     ; 'GIDX'
821d9560  stw  r7,0x50(r3)      ; +0x50 = 'GIDX'
821d9564  stw  r10,0x54(r3)     ; +0x54 = 0x10   the GIDX chunk size
821d9568  stw  r28,0x58(r3)     ; +0x58 = the identifier
```

Every offset matches what cycle 1163 measured, including the two that had been
derived separately — width at `+0x24` and height at `+0x26` are exactly the
`lhz +0x14`/`+0x16` of `0x8234B360` read against the descriptor base `0x10`, and
the data offset `0xFF0` is the value every wrapper in the corpus carries.

A writer is stronger evidence than a reader for a layout question. A reader shows
what the code tolerates; a writer shows what the format *is*, because these are
the bytes retail itself emits.

## The stride was never a constant to guess

Cycle 1163 measured a 0x50 record stride and said so - measured, not derived. It
is a **declared field**, the halfword at file `+0x1C`, and `0x821D9478` sets it
to `0x50` for the single-texture wrapper it builds.

Read across the 86 packs rather than assumed:

```
declared stride at +0x1C     80 in 82 packs,  128 in 4
declared eXt size at +0x44   32 in 82 packs
declared GIDX size at +0x54  16 in 82 packs
packs where the declared stride lands on GIDX for every record   82 / 86
```

So 82 packs are self-consistent under their own declared stride, which is the
check that matters: the field is not decoration, it drives the walk.

## The four that are not

Four packs declare a stride of 128, and their `+0x44` and `+0x54` read 65,536 and
16,352 — values that cannot be chunk sizes. Either those wrappers do not place
`eXt` at `+0x40`, or they are a different variant of the header. **Not
established, and not worked around**: the correct reading of a 128-stride pack is
unknown, and pretending 0x50 applies to them would be exactly the substitution
this cycle just removed from the other 82.

## What this changes

The pack directory moves from measured to derived for the 82. What is still
measured rather than derived is that a record's descriptor sits at its `eXt`
base + `0x20` and that its data offset is relative to that descriptor - the
writer above emits one record and so cannot show the second one's placement.
That is the remaining step, and it is a read.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
