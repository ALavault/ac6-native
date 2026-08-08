# Cycle 1147 — I filtered past the answer, and the parent assignment was the first row

## The correction

Cycle 1145 wrote, and I committed to the repository:

> An entity acquires a parent only if something assigns one, and **nothing on
> the load path does**.

That is false. `0x820A7B2C` assigns it, and it lives inside `0x820A7070` — the
unit constructor this product already ports, the single most relevant function
in the image to the question I was asking.

Worse than missing it: **my scan found it.** The `+0x188` store scan returned 26
rows and `0x820A7B2C` was the *first* one. I then piped the output through
`awk '$3 ~ /^822[5-9a-bA-B]/'` because I had convinced myself the object code
lived in the `0x822x` range, reported the two survivors, and reasoned from those.
The answer was on screen and my filter removed it.

The irony is exact. Cycle 1147 opened by building `Ac6FieldRead.java` precisely
because a text match had misled cycle 1145 about `+0xF0`. Having built an
instrument to stop trusting a grep, I then trusted a region filter I had no
evidence for. **An instrument does not help if the next step throws its output
away.**

## What the parent assignment actually is

```
820a7b0c  lbz  r11,0x18(r27)     ; the parent's Obj index, from the Obj record
820a7b10  cmplwi cr6,r11,0xff    ; 0xFF -> leave +0x188 null
820a7b14  beq  cr6,0x820a7b30
820a7b18  subf r11,r24,r11       ; less this record's own index in the loop
820a7b1c  add  r11,r11,r3        ; plus the base 0x8226F050 returned
820a7b20  addi r11,r11,0x2
820a7b24  rlwinm r11,r11,0x2,0x0,0x1d   ; times four
820a7b28  lwzx r11,r11,r30       ; the entity pointer array
820a7b2c  stw  r11,0x188(r31)
```

Byte `+0x18` of an Obj record names another Obj record; the arithmetic turns
that into a slot in the entity pointer array the constructor is filling. The
`0xFF` sentinel is the same one the container uses everywhere else for "absent".

## Does the conclusion survive?

Yes, and now for a reason that was measured rather than assumed. In Mission 01:

```
Obj records naming a parent (byte +0x18 != 0xFF)     27
Obj records with the 0xFF sentinel                  407
```

Every Obj record belonging to a unit whose first triple `position_placeholder`
would have returned carries `0xFF`. Units 0 through 9 — including the player and
the two carrying the formation-looking offsets `(-50, -6.25, 50)` and
`(0, -200, -1000)` — all have no parent. For them `0x8229AF80` still places
nothing, and their triple is still a frame that was never applied.

So the substance of cycles 1145 and 1146 stands: 95 units placed from their
first tag-2 order, 135 with no load-time position, the overview plot unchanged.
What changes is *why*. The mechanism is not absent; it is used by 27 records out
of 434, and I asserted absence when I should have measured rarity.

That distinction matters for what comes next. A parent-relative placement pass
is now a known, bounded piece of work — 27 records — rather than a mechanism
someone would have to discover from scratch.

## A refuted guess, recorded so it is not re-made

The same function writes `+0x15C`, the model resource pointer that step 2f of
the ladder depends on:

```
820a7944  lbz  r11,0x61(r28)     ; 0xFF -> no model
820a7954  bl   0x820a7eb0        ; allocate the resource object
820a7958  or   r30,r3,r3
820a795c  lbz  r4,0x61(r28)
820a7964  bl   0x8228e9b8        ; lookup keyed by that byte
820a7968  lbz  r4,0x62(r28)      ; and a second, when not 0xFF
820a797c  bl   0x8228e9b8
820a79a8  ...  virtual slot +0x10 on [manager+0x728]   ; bind
820a79c8  stw  r30,0x15c(r31)
```

The obvious reading is that `r28` is the unit record and bytes `+0x61`/`+0x62`
of it select the model. **Measured and refuted**: across all 230 unit records,
byte `+0x61` is 0 for every one and byte `+0x62` is 0 for every one — one
distinct value each, and never the 0xFF this code tests for. Whatever `r28`
points at when `0x820A7944` runs, it is not the unit record data block.

Establishing what it is means tracing `r28` through `0x820A7070`, which is a
long function with several reassignments of that register. That is the next
piece of step 2f, and it is written down here so the next cycle starts from the
refutation instead of the guess.

## Decided rather than asked

The 27 parented records are **not** implemented in this cycle. Placing them
correctly needs the parent's *staging* transform to be populated at the moment
the child is placed, and the port has no staging/commit ordering yet — it writes
positions once, at build. Adding a half-ordered version to capture 27 units
would put an unmodelled sequencing assumption underneath the 95 that are
currently right. The record of what it takes is worth more than the 27 markers.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  24/24 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_class_map.py ... --require J2              ->  class_map=pass vtables=811 rejects=1619
```

No behaviour changed in this cycle. The header comment in `retail_scenario.h`
that carried the false claim now carries the correction and the measurement,
which is the only artefact that had to move.
