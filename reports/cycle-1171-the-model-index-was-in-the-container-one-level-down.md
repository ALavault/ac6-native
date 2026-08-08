# Cycle 1171 — the model index was in the container all along, one node deeper

## The contradiction resolves, and my register tracking was the miss

Cycle 1168 published a contradiction: `r25` could not be both the first argument
and an object whose `[+0]` is dereferenced to a byte count. The instruction that
settles it is four before the loop head:

```
820a76c0  lwz r11,0x164(r1)
820a76c4  lwz r25,0x8(r26)      <-- r25 reassigned
820a76c8  cmplwi cr6,r25,0x0
820a76d4  lwz r11,0x0(r25)
820a76d8  lbz r11,0x0(r11)      ; the count byte
```

My search for the reassignment looked for `or r25,…` and for `r25` as a source.
It did not look for `lwz r25,…`. That is a sixth instrument-scope error of the
same family, and the narrowest yet — the right instruction was inside the range I
searched, in a form I did not ask for.

The manager is not lost: it lives in `0x154(r1)` and is reloaded *inside* the
loop at `0x820A7700`. The loop uses both registers for different things, which is
exactly the shape that made the windowed reading survive.

## What r28 actually points at

`r26` is the outer cursor, `[arg2 + 4] + i·12` — a 12-byte record per unit. `r25`
is **word 2 of that record**, a three-word list header written by `0x8232F380`:

```
8232f3bc  stw r11,0x0(r29)   ; +0x00 = the list node's data block
8232f3d4  stw r5,0x4(r29)    ; +0x04 = the 8-byte element array
8232f3dc  lbz r10,0x0(r11)   ; count = BYTE 0 of that data block
8232f3e8  stw r10,0x8(r29)   ; +0x08 = base + count*8, the 0x20-byte record array
```

which is the 8 / 0x20 pairing the loop walks, and `[[r25+0]+0]` is that count
byte. And `ObjBin::read` at `0x82330158` fills word 0 of each 0x20-byte record:

```
82330184  lwz r11,0x0(r30)   ; node[0] = a relative data offset
82330198  add r11,r11,r30
823301a0  stw r11,0x0(r27)   ; record+0x00 = that node's data block
```

with the node being the Obj entry's **child[0]** — not the entry itself.

## The measurement I got wrong, and why

Cycle 1148 measured byte `+0x61` across "all 434 Obj node data blocks" and found
one distinct value, zero. It concluded the model selector is **not in the
scenario container** and sent three later cycles hunting an external table.

It was reading the entry node. The record points at the entry's *child*. One
level down, over the same 434 records:

```
                     entry node (cycle 1148)      child[0]  (this cycle)
+0x61  distinct              1                        39,  0xFF in 123
+0x62  distinct              1                        39,  0xFF in 125
+0x56  distinct              1                        11,  zero in 213
+0x48  distinct              1                        47
```

Measured independently here and matching the delegated trace on all five fields.

**So the model index is in the scenario container.** Cycle 1148's conclusion is
withdrawn — and with it the framing of cycles 1155 and 1157, which went looking
for an external definition table. The MDLP those cycles found is still the
directory the indices point *into*; what was wrong was the belief that the
indices themselves were somewhere else.

## What the indices look like

`+0x61` takes 38 non-sentinel values: 0, 2, 4, 6, … 74, with 19 and 43 the only
odd ones. `+0x62` is `+0x61 + 1` in **281 of 434** records, and is never
non-sentinel when `+0x61` is `0xFF`. Every value is below 94, the MDLP's entry
count.

So the two bytes are a **pair of consecutive MDLP entries**, which is what
`0x820A7070` does with them: two `0x8228E9B8` lookups, the second guarded by its
own `0xFF` test. What the second entry *is* — a variant, a damaged model, a
level of detail, a shadow proxy — is not established and is not guessed here.

## A correction to cycle 1096

That report states all 434 `ObjBin` records carry `data+0x56 = 0`, "donc tous la
clé 0". Only **213 of 434** do; the field takes 11 distinct values, and the
factory key table at `0x820A8138` has keys 0–14. The 434 count and the 140/42/48
faction split in the same report are correct.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed. Nothing in the product read `+0x61`; the withdrawn
conclusion lived only in reports and in the ladder, which is updated.
