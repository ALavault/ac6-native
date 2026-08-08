# Cycle 1183 — the floats are read as a position, and it lands at +0xC0

## The read cycle 1182 named

`0x822A23D8`, in the Xenon corpus. It decodes there: `0x822A23F0` is
`stvx128 vr127,r1,r12`, exactly the VMX128 instruction that halts the canonical
export and exactly what cycle 1115 imported the second corpus to lift.

The dispatch is the shape `WORLD_POSITION_DEBT.md` recorded:

```
822a240c  lbz    r11,0x2a(r30)   ; the mode byte
822a2418  blt    cr6,0x822a27b0  ; mode 0
822a241c  beq    cr6,0x822a26dc  ; mode 1
822a242c  lbz    r5,0x2d(r30)    ; the anchor pair
822a2434  lbz    r4,0x2c(r30)
822a2448  bl     0x82270380      ; the unit lookup, as tag-2's resolver uses
```

and the mode-0 branch settles the question:

```
822a27b4  lfs    f0,0x4(r30)     ; +0x04  ┐
822a27d0  lfs    f0,0x8(r30)     ; +0x08  ├ assembled on the stack at r1+0x70
822a27d8  lfs    f0,0xc(r30)     ; +0x0C  ┘
822a27c4  lfs    f0,0x1348(r11)  ; w = 1.0, the constant cycle 1124 named
822a27e4  lvx128 vr0,r0,r10
822a27e8  stvx128 vr0,r31,r11    ; -> object + 0xC0
822a27ec  lfs    f2,0x1c(r30)    ; the two angles, to 0x822A1E80
822a27f0  lfs    f1,0x18(r30)
```

**The three floats at `+0x04`/`+0x08`/`+0x0C` are read as a world position.** Not
correlated with one — read, as a four-component vector with the same `1.0` w that
the object constructor uses, and stored as a vector. Cycle 1182's correlation is
now a derivation.

So the player's first order carries `(-2025, 1500, 1345)` and that triple reaches
a position slot. The same authored value the `PLAD` table holds and never reads.

## What it does not settle, and this matters

It lands at **`object + 0xC0`**, and `WORLD_POSITION_DEBT.md` records `unit+0xC0`
as **the destination** — not the live transform, which is `+0x20`/`+0x50`, nor
the staging block at `+0x70`/`+0xA0` that `0x8229BE98` commits.

So what is derived is that the triple is a *position*. Whether it is the spawn or
the first waypoint depends on what `+0xC0` is, and this cycle did not verify the
debt page's claim about that field — it inherited it, which is the habit that
produced four corrections this session.

Two readings survive and the evidence here does not choose between them:

- `+0xC0` is a destination, the tag-0 order is "fly to here", and the player's
  spawn is still unfound — but then `PLAD` and the first order agreeing on one
  triple wants explaining;
- the first order at mission start is what puts the unit in the world, and
  "destination" is the field's name in steady state rather than its only role.

## The next read, named as precisely as the last one was

What writes `object+0x20`/`+0x50` or `+0xA0` from `+0xC0`, or what reads `+0xC0`
at load rather than per-frame. That decides between the two readings, and it is
the same question as whether `initial_world_position` is placing 95 units at
their destinations.

**Nothing changed in the product.** The reversal cycle 1182 flagged is still
flagged, not applied.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
all four gates                                      ->  pass
```

## Addendum — one bounded step, and a deliberate stop

Searching for `+0xC0`'s consumers found two writers and no readers:

```
0x8229A510  stw  r30,0xc0(r31)   ; the object constructor 0x8229A470, r30 = 0
0x8229DBB8  stfs f0,0xc0(r31)
```

The first ties the field to the object class whose constructor cycle 1145 read —
the same one that zeroes `+0x184` and `+0x188`. So `+0xC0` is a vector slot on
that class, zeroed at construction, written by the resolver.

**No reader was found, and the search form is the reason to distrust that.** Two
displacement spellings were tried. A vector slot is more likely read with
`lvx128` against a computed base than with a literal `0xc0(rN)`, which is exactly
the encoding-coverage gap that produced this session's `lwz r25,` miss and its
`addi rX,rY,0x188` miss before that.

So this is not "nothing reads `+0xC0`". It is "two spellings found nothing", and
the distinction is the whole difference between cycle 1145's wrong claim and
cycle 1171's correction of it.

Stopping here on purpose. The remaining question — whether `+0xC0` feeds the live
transform at load — needs a reader search with an instrument that resolves
effective addresses, not another displacement guess.
