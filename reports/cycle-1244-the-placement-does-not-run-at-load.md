# Cycle 1244 — the placement does not run at load, and that is the answer

Cycle 1206 declined to change `initial_world_position` partly because *"the
`0x7D4` placement is not proven to execute at load"*. **That is now known to be
true rather than unknown: it does not execute at load, by construction.**

## The mechanism

The placement is a **parent-to-child push** that only a Set leader performs, from
inside its own order-execution FSM:

```
822a27b4..27e8  0x822A23D8 writes the Set matrix     leader+0x90/A0/B0, +0xC0
822a28b8  loop  r29 = [leader+0xD8][i] , i < [leader+0xDC]
822a28f0        rlwinm r11,[r29+0x118],0x1f,0x1f,0x1f   ; bit 0x2
822a2904          set   -> li r4,0x7d4
822a2928          clear -> li r4,0x7d1
8229c964  0x8229C920 case 0x7D1 -> re-sends 0x7D4 to itself
8229c9a8        ori r11,r11,0x3 ; stw r11,0x118(r31)   ; sets the bit above
8229cb80  0x8229C920 case 0x7D4 -> bl 0x8229adf8       ; unconditional
```

`0x7D1` is the **first** placement of a child and `0x7D4` every later one. The
`|= 3` at `8229c9a8` sets exactly the bit `822a28f0` reads — **a coupling that
could have been any other bit and is not.**

## The census, and the control that makes its zeros mean something

Force-scanned over 851,718 of 859,595 `.text` instructions: **three** `li rX,0x7d1`
and **three** `li rX,0x7d4`. Cycle 1218 listed one of the latter three;
`0x82297858` and `0x822A864C` are new, and both iterate the same
`[this+0xD8]` array.

Then the far-side query, re-run here:

```
blocks=12  bytes=11,117,714
  0x000007D4 = 0 aligned / 0 unaligned
  0x000007D1 = 0 aligned / 0 unaligned
  0x8229C920 = 7 aligned            <- the control
```

**The message codes exist nowhere in the image as data** — not in a table, not in
a vtable, nowhere. So a register-held send at any of the 225 slot-`+0x24`
dispatch sites must still originate at one of those six `li` instructions, and
the enumeration is complete rather than a lower bound. The `0x8229C920` control
returning 7 is what makes the two zeros evidence instead of an empty instrument.

## The loader builds both sides and fires neither

`0x820A7070`, which `0x8219F8C0` calls three times, writes on each unit
`+0x184` (the formation offset the placement consumes), `+0x170` (the Set index),
`+0x118` (the flag `822a28f0` tests) and `+0x188` (the parent) — **and on the
leader** `+0xD8`, `+0xDC`, `+0xE0`, `+0xE4`, the child array and order list.

Every input the push needs is wired at load. The push is not performed.

## A false positive, caught, and it is the rarer direction

The first reachability graph found a path from the loader to the placer:
`0x8219F8C0 → … → 0x8224C4D0 -b→ 0x822A2BE8 → 0x822A2B50 → 0x822A23D8`. **It is
false.** `0x8224C4D0` ends `blr` at `8224c510`; the `b` at `8224c55c` belongs to a
**different function that `.pdata` has no entry for**. Rebuilding boundaries as
`.pdata ∪ bl-targets` — 8,157 to 10,467 starts — killed the edge and every other
path.

Every trap this file has recorded produced a **false negative**. This one
produced a **false positive**, which this repository has far less practice
catching: a negative gets challenged, a positive that confirms what you hoped
does not.

## What it means for the product, stated plainly

The formula is confirmed: `world = SetMatrix · desc[+0x184] + SetTranslation`,
and `8229ae7c` is the only thing that ever writes `unit+0xA0`. What is not true
is that it happens during the load.

**So `initial_world_position` should not be "fixed" to apply it at load.** The
honest framing is *applied at first update*, and the product's session loop is
where that would live — which is a different change from the one cycle 1206
contemplated, and a larger one.

Task 13 is updated to say so rather than left describing a change that would be
wrong.

## Not established, stated plainly

- **What starts the leader's FSM.** `0x82297540` has zero instruction references
  and appears in data only in `.pdata`; every path into it found is a transition
  from a sibling state. **This is the single open hop**, down from the four cycle
  1206 left.
- That the tag-0 order is the *first* executed. `0x82296E40` dispatches on a
  virtual's return and `0x822A2B08` takes an explicit index; nothing read fixes
  the order. Cycle 1206's "the Set's first order" is a container-layout claim.
- That `[unit+0x118] & 0x2` is clear at construction — `820a7a54..7a68` touches
  bit `0x10`, not `0x2`. If it is not clear the first send is `0x7D4` instead of
  `0x7D1`; **the placement is the same either way**, so the conclusion survives
  and the first-versus-repeat reading does not.
- The six receiver vtables have **no RTTI** — verified after fixing a COL reader
  that had failed its own known-good — so their identity rests on field overlap,
  which is an inference from offsets rather than a type read.
- Cycle 1206's "Set 0 is the player's Set" is untouched and still convergent.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
0x7D4 and 0x7D1 as data words: 0 of 11,117,714 bytes; control 0x8229C920 = 7
```

No product code changed — **deliberately, and now for a proved reason.**
