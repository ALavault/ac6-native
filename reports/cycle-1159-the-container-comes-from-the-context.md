# Cycle 1159 — the model container is supplied by the context, through a vtable slot

## Where the container comes from

Cycle 1158 left two gaps. This closes one of them: what fills the three-word
container at `r1+0x80` that `0x8228E9B8` indexes.

```
820a70b4  lwz  r11,0x0(r25)    ; r25 is 0x820A7070's first argument - its vtable
820a70bc  addi r4,r1,0x80      ; r4 = &container
820a70c0  lwz  r11,0xc(r11)    ; vtable slot +0x0C
820a70c4  stw  r24,0x80(r1)    ; the three words, zeroed first
820a70c8  stw  r24,0x84(r1)
820a70cc  stw  r24,0x88(r1)
820a70d0  mtspr CTR,r11
820a70d4  bctrl                ; r25->vtable[+0x0C](r25, &container)
```

So the container is not built from a file inside this function and is not a
global. **The context hands it over**, through virtual slot `+0x0C`, and
`0x820A7070` then enumerates it, registers every entry, and later indexes it by
the model bytes.

That is why cycle 1158 could not settle the container's provenance by reading
`0x820A7070`: the answer is not in `0x820A7070`. It is in whichever class the
caller passes as `r3`, and the binding is polymorphic by construction.

## What this does and does not add

**Adds.** The model index space belongs to the *context*, not to the unit loader.
A mission supplies its own bundle index, which is consistent with Mission 01
carrying its own 94-entry MDLP in its own bundle (cycle 1157) rather than the
game holding one global model table.

**Does not add.** It does not identify the concrete class behind `r25`, and it
does not connect that slot to the MDLP file. The structural match of cycle 1158
stands exactly where it stood: `0x8228E9B8`'s arithmetic *is* the MDLP's layout,
and that is not the same claim as *this container is that file*.

## The other gap is unchanged, and its instrument is now checked

Byte `+0x61` remains unlocated. Cycle 1148 measured it as zero across all 230
unit record data blocks, and this cycle checked that instrument the way cycle
1151 checked the Obj one: the unit record data blocks are sorted and their
consecutive gaps measured, and the **smallest gap in the mission is 848 bytes**,
against the `0x63` needed to reach the field. Every read was inside its own
block, so the zeros are real and the record is genuinely a third structure.

## Decided rather than asked

Still nothing written into the product. The next step is the caller of
`0x820A7070` — which class it passes — and that is a read, not a guess.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed.
