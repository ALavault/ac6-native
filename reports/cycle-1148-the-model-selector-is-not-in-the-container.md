# Cycle 1148 — the model selector is not in the scenario container

## The question

Step 2f of the ladder wants the class→model binding **derived**, through the
`+0x15C` resource pointer each constructed object carries, rather than written
by hand. Cycle 1147 found where `+0x15C` is filled and refuted the obvious guess
about where its selector comes from. This cycle finishes that question.

## What fills +0x15C

Inside `0x820A7070`, per record:

```
820a7944  lbz  r11,0x61(r28)     ; 0xFF -> no model, skip the whole block
820a7954  bl   0x820a7eb0        ; allocate the resource object
820a7958  or   r30,r3,r3
820a795c  lbz  r4,0x61(r28)
820a7964  bl   0x8228e9b8        ; lookup keyed by that byte
820a7968  lbz  r4,0x62(r28)      ; and a second, when not 0xFF
820a797c  bl   0x8228e9b8
820a79a8  ...  virtual +0x10 on [manager+0x728]    ; bind
820a79c8  stw  r30,0x15c(r31)
```

## Where r28 comes from

The loop walks two parallel arrays out of the same container struct `r25`:

```
820a76f8  lwz  r11,0x8(r25)      ; base of the 0x20-byte record array
820a7708  add  r26,r22,r11       ; r22 strides 0x20  (820a7c08)
820a770c  lwz  r28,0x0(r26)      ; r28 = word 0 of the 0x20-byte record
...
820a779c  lwz  r10,0x4(r25)      ; base of the 8-byte element array
820a77b8  lwzx r27,r10,r23       ; r23 strides 8     (820a7c0c)
820a7a1c  stw  r27,0x184(r31)    ; r27 is the Obj node data - cycle 1142's chain
```

So `r27` is the Obj node's data block, which is container memory and whose byte
`+0x18` cycle 1147 read as the parent index. **`r28` is a different pointer** —
word 0 of the neighbouring 0x20-byte record — and it is read at `+0x48` (a
halfword), `+0x56`, `+0x61` and `+0x62`.

## The measurement

Two structures in the container could plausibly be what `r28` points at. Both
were checked at those exact offsets:

| structure | count | `+0x48` | `+0x56` | `+0x61` | `+0x62` |
|---|---:|---|---|---|---|
| unit record data blocks | 230 | — | — | 1 distinct value: `0` | 1 distinct value: `0` |
| Obj node data blocks | 434 | 1 distinct: `0` | 1 distinct: `0` | 1 distinct: `0` | 1 distinct: `0` |

Every one of them is zero, and never the `0xFF` sentinel this code tests for
first. The selector is not in either.

**The instrument was checked before the result was believed.** A zero at `+0x61`
would mean nothing if the blocks were shorter than `0x62` bytes — the read would
have been past the end. The Obj data blocks are sorted and their consecutive
gaps measured: the **smallest gap in the whole mission is 352 bytes**, and the
distribution runs from there to 26,432. Every read was comfortably inside its
own block.

A second, weaker check points the same way. The code special-cases two asset IDs
at `+0x48`, `0x2CF2` and `0x2CF4`; neither byte pattern occurs anywhere in the
3,477,248-byte container. That alone would only show those two IDs are unused by
Mission 01, which is why it is not the argument — the offset sweep above is.

## What this means for step 2f

**The class→model binding cannot be derived from the scenario container alone.**
The record carrying the selector is external — a unit or aircraft definition
table loaded from elsewhere in the retail data, reached through word 0 of the
0x20-byte record array.

That changes the shape of 2f rather than blocking it. The ladder's warning still
holds and now has a sharper edge: a hand-written class→model table would satisfy
the gate and lose the property, and the temptation to write one is *stronger*
now that the honest route needs a second data source. The derived route is:
identify the definition table, parse it, key it by the byte the container does
supply, and cite the whole chain.

What the container *does* supply for each unit is the class byte at data `+0x08`
(230 records, four distinct values on Mission 01: 0, 1, 2, 4) and the faction
byte at `+0x0D`. The model comes from the other side of a join this workspace
has not yet located.

## Decided rather than asked

No parser or binding is written this cycle. Writing one against a table I have
not identified would be inventing a schema, and the ladder already names this
step as the place where JV quietly becomes J1 again. A located negative is worth
more here than a plausible parser.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  24/24 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_class_map.py ... --require J2              ->  class_map=pass vtables=811 rejects=1619
```

No product code changed. This cycle is a measurement and a boundary.
