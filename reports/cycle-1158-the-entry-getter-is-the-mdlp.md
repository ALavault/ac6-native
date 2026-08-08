# Cycle 1158 — the entry getter is the MDLP, and the model index space is derived

## The getter

`0x8228E9B8`, five instructions of arithmetic and a bound check:

```
8228e9b8  lwz    r11,0x0(r3)     ; container[0] = the header
8228e9bc  lwz    r11,0x4(r11)    ; header +0x04 = the entry count
8228e9c0  cmplw  cr6,r4,r11
8228e9c4  blt    cr6,0x8228e9d0
8228e9c8  li     r3,0x0          ; index >= count -> null
8228e9cc  blr
8228e9d0  lwz    r11,0x4(r3)     ; container[1] = the offset table
8228e9d4  rlwinm r9,r4,0x2,...   ; index * 4
8228e9d8  lwz    r10,0x8(r3)     ; container[2] = the data base
8228e9dc  lwzx   r11,r9,r11      ; offset = table[index]
8228e9e0  add    r3,r11,r10      ; entry = base + offset
8228e9e4  blr
```

Set that beside the MDLP header cycle 1157 validated on Mission 01's own file:

| the getter reads | the MDLP has |
|---|---|
| entry count at header `+0x04` | `+0x04` = **94** |
| an offset table, stride 4 | table at `0x1000`, 94 monotonic dwords |
| a data base added to the offset | base `0x2000`; `base + table[i]` lands on `FHM ` for **94 of 94** |

The arithmetic and the file were established independently — the getter by
disassembly, the layout by parsing the file and checking three closures — and
they are the same structure.

## The index space, which is the part that mattered

Step 2f needed the join from a unit's class to a model to be **derived**, because
a hand-written table passes the gate and destroys the property the gate exists
for. `0x820A7070` supplies it, in two places that share one container.

First, before the per-record loop, it walks the whole container:

```
820a7178  bl 0x8228e9a8       ; count(container)
820a7184  or r4,r31,r31       ; index
820a7188  addi r3,r1,0x80
820a718c  bl 0x8228e9b8       ; entry = get(container, index)
820a71a4  lwz r11,0x18(r10)   ; register it - virtual +0x18 on [manager+0x728]
820a71ac  bctrl
820a71b4  addi r31,r31,0x1
820a71b8  bl 0x8228e9a8
820a71c0  blt cr6,0x820a7184  ; while (++index < count)
```

`0x8228E9A8` is the count getter; the loop bound is unambiguous from its use.
So every entry of the container is registered with the resource manager at load.

Then, per record, it indexes **the same container at `r1+0x80`** with the two
bytes cycle 1147 found:

```
820a795c  lbz  r4,0x61(r28)
820a7960  addi r3,r1,0x80
820a7964  bl   0x8228e9b8
820a7968  lbz  r4,0x62(r28)
820a797c  bl   0x8228e9b8
820a79c8  stw  r30,0x15c(r31)   ; the object's model resource
```

All four calls to `0x8228E9B8` in the image are in this function and all use
`r1+0x80`. **So bytes `+0x61` and `+0x62` are indices into the same entry space
the registration loop enumerates** — the MDLP's, whose count for Mission 01 is
94 and whose bound is the one `0x8228E9B8` checks.

That is the join, and it is derived rather than fitted: no ordinal was matched
against a unit class and inspected for plausibility.

## What is still not established

**Which record holds `+0x61`.** `r28` is word 0 of the 0x20-byte record array
(cycle 1148), and cycle 1148 measured that byte `+0x61` is zero in all 230 unit
record data blocks and all 434 Obj node data blocks. So the byte exists on a
third structure this port has not identified. Until it is, the join is a shape
without an input.

**The container's provenance.** `r1+0x80` is a stack triple built earlier in
`0x820A7070`; I did not trace what fills it. The structural match to the MDLP is
exact and the registration loop is consistent with it, but "the getter's
arithmetic is the MDLP's layout" is not the same statement as "this container is
that file", and I am not merging them.

## Decided rather than asked

Nothing is written into the product. The remaining gap is a single unidentified
record, and the honest next step is to find it — not to note that 94 entries and
a byte index fit together neatly and start writing a resolver against that fit.
Cycle 1157 said the same thing about entry ordinals; the temptation has simply
moved one field along.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed.
