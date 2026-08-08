# Cycle 1187 — `+0xC0` is written by both order kinds, and a substring is not a regex

## The instrument error, first

Searching for `li rX,0xc0` — the form a vector offset takes, since `+0xC0` is
never a literal displacement — returned **zero hits**. I had read
`822a27c8 li r11,0xc0` ten minutes earlier.

`Ac6XenonRefs` does `text.toLowerCase().contains(pattern)`. It is a **substring**
matcher, and I gave it `,0xc0$` — regex syntax, so it searched for a literal `$`
and correctly found none.

That is the eighth instrument-scope error of this session and the first I caught
*before* believing it, purely because I had a known-good hit to check against.
Every earlier one was caught later, by a contradiction. **A search with no
expected answer cannot be validated**, and this one had one by accident.

Redone as a plain substring, with the known site as a sanity check: 1 hit at
`0x822A27C8`, and the search returns real results.

## What it found

`li rX,0xc0` in the object region, all followed by a vector access at
`r31 + 0xC0`:

```
0x82295F7C  ->  82295f88  stvx128 vr124,r31,r11     the tag-2 order region
0x822961F4                                          the same family
0x822967BC      in 0x82296650
0x822A25F4  }
0x822A2728  }   the three mode branches of 0x822A23D8, the tag-0 resolver
0x822A27C8  }
```

`0x82295F88` is a **write**, and it sits in the same function family as the tag-2
order switch `0x82295A88` and its resolver call `0x82295BF0`.

## Which settles cycle 1183's question, in the unwelcome direction

Cycle 1183 derived that the tag-0 payload's floats are read as a position and
stored to `object+0xC0`, and left two readings open: `+0xC0` is the destination,
or the first order is what places the unit.

**Both order kinds write it.** A slot rewritten by every movement order is not a
spawn slot. `WORLD_POSITION_DEBT.md`'s inherited label — `unit+0xC0` the
destination — now has a reading behind it rather than being taken on trust.

So the tag-0 triple is the player's **first destination**, not its spawn. The
same authored value `PLAD` holds, still unread by `PLAD`'s own accessor, reaching
the player as a first waypoint.

## What this does to the flagged reversal

Cycle 1182 flagged that `initial_world_position` might be placing 95 units at
their destinations rather than their spawns, and offered the tag-0 order as the
real spawn. **That offer is withdrawn**: tag-0 and tag-2 write the same slot, so
they are the same kind of thing. If tag-2 gives destinations, so does tag-0, and
swapping one for the other would have moved the problem rather than fixed it.

The flag itself stands. The 95 positions may still be destinations. What is gone
is the candidate replacement — which is worth more than it sounds, because
cycles 1182 and 1183 were building toward changing ported code on it.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
all four gates                                      ->  pass
```

No product code changed, and now none is going to be on this evidence.
