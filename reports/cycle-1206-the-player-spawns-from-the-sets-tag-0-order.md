# Cycle 1206 — the player spawns from the Set's tag-0 order, and cycle 1145 read the wrong order

## The correction, which is the important part

Cycle 1145 replaced the `Obj` triple with **the first tag-2 order of the unit's
program**, and `initial_world_position` has used that ever since — 95 of 230
units placed, 135 refused. That reading is wrong, and cycle 1182 was right to
flag it.

**The tag-0 order is per-*Set* and carries the Set's spawn transform. The
per-unit record is a formation offset, not an absolute position.**

```
82296f94  lwz  r30,0x0([order+4])   ; the tag-0 payload
82296fac  bl   0x822a23d8
822a240c  lbz  r11,0x2a(r30)        ; mode byte
822a2418  blt  cr6,0x822a27b0       ;   mode 0
822a27b4  lfs  f0,0x4(r30)          ; x
822a27d0  lfs  f0,0x8(r30)          ; y
822a27d8  lfs  f0,0xc(r30)          ; z
822a27e8  stvx128 vr0,r31,r11       ; runner + 0xC0
822a27ec  lfs  f2,0x1c(r30)
822a27f0  lfs  f1,0x18(r30)
822a27f4  bl   0x822a1e80           ; and the rotation half
```

`0x822A1E80` writes identity rows to `runner+0x90/+0xA0/+0xB0` and then composes
three rotations into them. **So `+0xC0` is the translation column of a 4×4 at
`runner+0x80`, not a standalone destination** — which corrects cycle 1183, that
read the translation and stopped.

The matrix reaches the unit through the `0x7D4` message:

```
8229ae04  lwz  r10,0x170(r3)   ; the SET index 0x820A7070 stored
8229ae34  lwzx r10,r10,r9      ; the runner array at [*0x826E4EB4 + 0x2D3B4]
8229ae44  addi r6,r10,0x80     ; the matrix 0x822A23D8 wrote
8229ae60..ae74  src+0x90/A0/B0 -> unit+0x70/+0x80/+0x90
8229ae78  lvx128  vr0,r6,0x40  ; src+0xC0
8229ae7c  stvx128         -> unit+0xA0     <- the spawn position lands here
8229ae80  lwz  r9,0x184(r3)    ; the per-unit descriptor
8229aea4..af44  rotate desc+0x00/+0x04/+0x08 by that matrix, add to unit+0xA0
```

The per-unit descriptor is added **after** rotation by the Set's matrix. That is
a formation offset by construction.

## Why so many units looked unplaceable

`0x820A7070` writes the parent only when the descriptor's byte `+0x18` is not
`0xFF`:

```
820a7b0c  lbz   r11,0x18(r27)
820a7b10  cmplwi r11,0xff  -> skip
820a7b2c  stw   r11,0x188(r31)
```

Over 434 units that byte is `0xFF` **407 times**, with `0,1,2,3,4,6,9` for the
other 27 — small in-Set indices, never out of range. **A unit that leads its
flight has no parent by construction**, so `0x8229AF80`'s parent path is the
minority path and `0x8229ADF8` is the normal one. The 135 "unplaced" units were
never a defect in the data.

## The player

Set 0, one unit. Its tag-0 payload at container offset `0x510`:

| field | value |
|---|---|
| `+0x04/08/0C` | **(-2025.0, 1500.0, 1345.0)** |
| `+0x18`, `+0x1C` | `-0.261799` (-15°), `-1.221730` (-70°) |
| `+0x2A` | `0` — mode 0 |
| `+0x0B` | `0x00` — not `0xFF`, so `0x82299B44` sets the placement bit |
| unit descriptor `+0x18` | `0xFF` — no parent |
| unit ObjBin `+0x61/+0x62` | `0xFF`, `0xFF` — no model from the container |

This is the same triple as Mission 01's PLAD row — **the authored value cycles
1146, 1177 and 1179 proved PLAD's own accessors never read.** It reaches the unit
through the Set's first order, not through PLAD.

## The aircraft is not in the mission, and the expression is exact

`0x820A8678` never touches `+0x61`. It builds `"DPL::[%#x,%#x]"` from ids
resolved out of the settings object:

```
821b701c  id = 0x15A + 4*p            (0x196 + 4*p when slot != 0)
82090438  p  = *(u32*)(settings + 0xAD3C + profile*0xA7E0 + slot*12)
          settings = *(0x826E4EB4) + 0x70
820a8820  stw r3,0x15c(r23)           ; the resolved model -> object+0x15C
```

Cycle 1177's *hypothesis* — "the mission cannot name the player's aircraft" — is
now a read fact, with the address of the word that does name it.

## Controls, five that could have failed

1. **Strides, three ways.** `0x820A7070` walks 12 / `0x20` / `0x08`;
   `0x8232CCA0` builds 12-byte Set entries and `0x8232F380` allocates `count<<5`
   and `count<<3` at `8232f3f4` / `8232f3e0`.
2. **`unit+0x184` is a formation offset.** Read across all 434 units it gives
   round metric values — `(-50, -6.25, 50)`, `(0, -200, -1000)`,
   `(500, 0, -1000)` — and exactly `(0,0,0)` for each Set's lead unit. Garbage
   was the null result and did not occur.
3. **`desc+0x18` is a leader index**, by the histogram above.
4. **The `0x510` payload matches the code's field map exactly** — mode `0` at
   `+0x2A`, `FF FF` anchors at `+0x2C/2D`, `+0x0B` non-`0xFF`. Any one
   disagreeing would have killed the chain.
5. `Ac6XenonRefs` was run with a known-good control every time; `Ac6Xrefs` was
   not used, per cycle 1193.

## Not established, stated plainly

- **That Set 0 is the player's Set was not derived from code.** The evidence is
  convergent — it is Set 0, its tag-0 order carries the Mission 01 PLAD triple
  exactly, its single unit has `+0x61 = +0x62 = 0xFF` — but no instruction was
  read that selects it. **If Set 0 is not the player's Set, the position above is
  wrong**; everything else here survives.
- **The kind selector is unresolved and was not banked.** `r15` comes from a
  five-way switch at `0x820A72C0` on byte `+0x08` of the Set-data payload, which
  reads `0` for all 230 Sets in the slot-0 container — which would route every
  unit through the player's DPL path, plainly false since 311 of 434 carry a real
  `+0x61`. Either `arg2` is runtime-assembled rather than parsed slot 0, or the
  byte is patched during load. **So the player taking the `0x820A8678` branch is
  an inference, not a read.**
- **Reachability at load.** `0x8229C920` case `0x7D4` performs the placement and
  `0x7D1` self-sends it, but who sends `0x7D1`, and which of the five
  `0x8229AF80` call sites runs during mission load, was not established. Per the
  cycle-1193 rule, treat the placement as derived but not proven to execute.
- `[globalobj+0x2D3B4][setIndex+1]` was identified as the Set runner **by layout**
  — the `+0x80` matrix and `+0xC0` translation matching — not by reading what
  populates the array.

## A schema correction owed

`analysis/scenario-schema/ObjBin.json`'s `reach_path` and `SetBin.json` describe
the `0x08` array as holding `SetBin` records. `0x8232F198` actually stores the
Obj entry node's **own** data payload at `rec8[0]` (`8232f1dc`) and a `SetBin`
record at `rec8[4]` (`8232f294`). The distinction matters: `SetBin::read` puts an
act count at byte 0, which cannot coexist with a float, and that near-contradiction
almost sank the `+0x184` finding.

## Decided rather than asked

**Nothing is changed in `initial_world_position` yet.** The correct source is now
known, but the placement is not proven to execute at load and Set 0's identity as
the player's Set is convergent rather than derived. Rewriting the product's
placement on that basis would trade a wrong reading for an unproven one, which is
the trap cycles 1195 and 1203 were about. The debt is recorded; the fix waits for
the two open reads.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed. Container payload SHA-256 `51c10abe…45ac6d45`.
No oracle used.
