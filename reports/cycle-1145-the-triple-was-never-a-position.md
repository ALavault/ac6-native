# Cycle 1145 — the Obj triple was never a position, and the frame that would have made it one

## What this cycle set out to do

Step 0.2 of the ladder: find the frame the unit placement is relative to.
Cycle 1142 had the chain — the Obj triple reaches the live transform through
`entity+0x184`, `0x8229AF80` and the commit at `0x8229BE98` — but not the frame,
because `0x8229AF80` tests `[entity+0x188]` before writing and nothing said what
that parent was. `WORLD_POSITION_DEBT.md` recorded it as "the value is right and
its frame is not".

The frame was found. Then the frame made the value wrong.

## The frame

`0x8229AF80`, with `r11 = [entity+0x188]`:

```
8229afc0  addi r11,r11,0x60          ; r11 = parent + 0x60
8229affc  addi r5,r1,0x50            ; the offset triple, on the stack
8229aff4  addi r10,r11,0x10          ; parent + 0x70   basis row X
8229b000  addi r9,r10,0x10           ; parent + 0x80   basis row Y
8229b008  addi r7,r10,0x20           ; parent + 0x90   basis row Z
8229b004  lfs f13,0x40(r11)          ; parent + 0xA0   translation x
8229b00c  lfs f12,0x44(r11)          ; parent + 0xA4   translation y
8229b010  lfs f11,0x48(r11)          ; parent + 0xA8   translation z
8229b044  vmsum3fp128 vr0,vr11,vr12  \
8229b048  vmsum3fp128 vr11,vr10,vr12  >  three dot products: basis * offset
8229b04c  vmsum3fp128 vr13,vr13,vr12 /
8229b064  fadds f13,f13,f10          \
8229b070  fadds f13,f12,f13           >  plus the parent's translation
8229b07c  fadds f13,f11,f13          /
8229b090  stvx128 vr0,r3,r6          ; -> entity + 0xA0
```

So `child_world = parent.translation + parent.basis * offset`, read off the
parent's **staging** rows at `+0x70/+0x80/+0x90/+0xA0` — the same staging half
that cycle 1134 established and that `0x8229BE98` later commits into
`+0x20..+0x50`. That is the frame, and it is an ordinary parent-relative
placement.

## Why finding it settled the opposite question

The function's first act is to refuse:

```
8229af9c  lwz r11,0x188(r3)
8229afa4  beq cr6,0x8229b100     ; null parent
...
8229b100  li r3,0x0              ; and the whole function does nothing
```

and the constructor at `0x8229A470` — which is where an entity's fields get
their initial values — zeroes both of them:

```
8229a5ac  stw r30,0x184(r31)     ; r30 = 0
8229a5b0  stw r30,0x188(r31)
```

An entity acquires a parent only if something assigns one, and nothing on the
load path does: of the 26 `stw` to `+0x188` in the image, exactly two are in the
object region, and one is this constructor's zero. So an unparented entity is
**never placed by this route at all**, and its Obj triple is an offset in a
frame that was never applied.

Which is worth stating plainly: `0x8229AF80` is not the units' placer. It is the
attach-to-parent placer, and the units have no parent.

## The measurement that should have come first

169 of Mission 01's 230 units have `(0, 0, 0)` as their first Obj triple.

```
units=230  no_obj=0  obj_zero=169  obj_nonzero=61
obj_scalars per record: 1->161  2->12  3->10  4->35  5->5  6->3  8->3  12->1
```

`position_placeholder` was not returning an offset with the wrong frame for
three quarters of the world. It was returning nothing at all, dressed as a
coordinate. Cycle 1144's capture — ten markers of 230 — was that fact rendered,
and I read it as a framing problem because that is what the previous cycle's
note had primed me to read.

## Where the load-time position actually is

The unit's `Set -> Act -> Order` program. `0x82295A88` switches on the order
record's `+0x45`:

```
82295b6c  lbz r11,0x45(r30)
82295b70  cmplwi cr6,r11,0x9
82295b74  bgt cr6,0x82296500
```

over ten arms. The arms for 5, 8 and 9 do their own arithmetic; **every other
value falls to the default arm**, which is the one that calls the resolver this
product already ported:

```
82295bd4  or   r5,r30,r30       ; the order's position record
82295bf0  bl   0x822953f0
```

So the first tag-2 order in a unit's program is a world position, resolved by
`0x822953F0` — mode 0 outright, mode 1 against an anchor.

## Coverage, and a cross-check that is not a restatement

`initial_world_position` answers for the units whose first such order
`0x822953F0` can resolve without an anchor and without the height query, and
refuses for the rest. On Mission 01:

| | units |
|---|---|
| placed | **95** |
| no load-time position in the container | **135** |
| total | 230 |

Of the 95, **94 fall inside the union of the rectangles the four sub-missions
install** (`x[-50000, 50000] z[-50000, 50000]`); the one outside misses by 168
units on x. That is a real check rather than a restatement, because the
rectangles are parsed from the sub-mission setup steps through `FUN_82268B28`
and these coordinates from the behaviour programs — two different parts of the
container, agreeing.

The spawns span `x[-50168, 16288] y[0, 9000] z[-16928, 2096]`, and the aircraft
sit at y = 1500.

## PLAD is not the player's spawn

The player is one of the 135. Its program has no tag-2 order, so the obvious
candidate was `PLAD` — Step 0.1's cross-mission table gives Mission 1
`(-2025, 1500, 1345)`, at exactly the 1500 the aircraft spawns use, which is
precisely the kind of coincidence that reads as proof.

It is not. `0x82249BC8` is the record getter (`base + index * 0x10`), and it has
three callers in the image — `0x82097FC8`, `0x8219C840`, `0x821A0328`. All three
do the same thing:

```
821a0338  lwz r11,0xc(r3)        ; word 3 only
821a0348  stw r11,0xf0(r10)      ; -> the route cursor
```

**No caller reads the floats.** The three coordinates in a PLAD record have no
consumer anywhere in the image reachable from the record getter. The plausible
rule is refuted, not confirmed, and this is recorded because a later cycle will
find that table again and have the same idea.

## What the capture now shows

`world_markers_live` went from **10 to 4**, and that is the honest direction.
The ten were units drawn at the origin because the origin was what the
placeholder returned. The four are units that are genuinely in view. The other
91 placed units are thousands of world units away, and the camera is still the
rasteriser's hardcoded fallback, sitting at the origin because the flight
integrator starts there.

So the blocker is now exactly the next item on the ladder — **2c, a flight
camera** — and it is blocked in turn on the player's own spawn, which this cycle
has shown is neither in the player's behaviour program nor in PLAD.

## Decided rather than asked

- **An unplaced unit is not drawn, and is not moved to the origin.** It stays in
  the world — the container built it, and the 230 count is JF-cited evidence —
  but `RetailWorld` now carries `placed` and `unplaced`, the renderer skips the
  latter, and the artefact publishes both counts. The origin is not a fallback;
  it is a different claim.
- **`initial_world_position` refuses mode 1 and the height flag** rather than
  guessing an anchor. That refusal is the coverage boundary and it is why the
  number is 95 and not 122.
- **`position_placeholder` is kept**, still exercised by the parser tests, so
  the waves-manifest generator keeps emitting what it always emitted and the two
  generators stay comparable. It is simply no longer on the session path.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  24/24 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_class_map.py ... --require J2              ->  class_map=pass vtables=811 rejects=1619
```

The placement partition is asserted in `retail_playable_tests.cpp` before the
artefact is written, and `placed + unplaced == published` is asserted with it,
so neither number can drift without the other being caught.
