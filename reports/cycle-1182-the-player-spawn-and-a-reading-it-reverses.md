# Cycle 1182 — the same authored triple in a second file, and the reading it reverses

## Cycle 1180's lead is closed, negative

The seven grandchildren of the player's Obj entry child are **eight zero bytes
each**. Every byte from `0x880` to `0x8EF` is zero.

And the control that makes that a real negative rather than a shrug: the
seven-slot table is fixed arity — all 434 Obj entries have exactly seven — and
**sixteen units with known tag-2 world positions have all seven absent too**
(units 45–60). A unit whose position is known carries nothing there, so the
slots are not the position carrier for anybody. When present they hold
per-airframe parameters: `0.2618, 0.5236, 0.3491` (15°, 30°, 20°), and a block
with `9.8` in it.

## What is there instead

The player's **first order** — `Set → Act 0 → Order 0`, tag 0 — resolves to a
0x30-byte payload at `0x510`:

```
+0x04  -2025.0      +0x18  -0.2618  = -15°
+0x08   1500.0      +0x1C  -1.2217  = -70°
+0x0C   1345.0      +0x2A  mode 0   +0x2C/+0x2D  FF FF
```

`(-2025, 1500, 1345)` is **exactly** Mission 1's `PLAD` row. Verified here
against `analysis/plad-campaign.tsv` and against the bytes.

Cycles 1146, 1177 and 1179 established exhaustively that `PLAD`'s floats have no
consumer. That was a negative about *`PLAD`'s accessor*, and it stands. It was
never a negative about the container. **The same authored triple lives in a
second, independent file**, in the player's first order, in a record carrying the
mode-and-anchor shape `0x822A23D8` resolves — the function
`WORLD_POSITION_DEBT.md` already records as reading a mode byte at `+0x2A` and an
anchor pair at `+0x2C/+0x2D`.

Four independent byte-level controls, over all 230 units:

| control | result |
|---|---|
| mode vs anchors | mode 0 → 90/95 anchors are `FF FF`; modes 1 and 2 → **0 of 135**. Perfect partition. |
| mode vs zero triple | mode 0 → 94/95 non-zero; mode 1 → 72/95 exactly `(0,0,0)` — the same semantic as tag-2 mode 1 |
| world rectangle | 90 of 95 mode-0 triples inside ±50000 |
| uniqueness | 110 units have a tag-0 triple shared with no other unit |

## The reading this reverses

`initial_world_position` takes each unit's **first tag-2 order** as its spawn.
For unit 9 that is `(1856, 1500, -16416)` — and sixteen *ground* units at y ≈ 60–85
carry the same x/z. Four aircraft do not stack on one point; that is an airbase
the flight is ordered toward. Units 9–12 are a four-ship whose tag-0 triples are
four distinct points sharing one heading (100.000°), and units 13–16 are a second
four-ship 3600 units behind.

So the tag-2 first order is plausibly a **destination**, and the tag-0 order the
**spawn**. The 95-of-230 placement cycle 1145 built, and the overview plot, rest
on the tag-2 reading.

**Nothing is changed in the product on this cycle.** The evidence is strong and
it is correlation: `0x822A23D8`'s body was not read — its export halts on a
VMX128 instruction at `0x822A23EC`, the same blocker cycle 1115 lifted for
`0x822953F0` by importing under `PowerPC:BE:64:Xenon`. Reversing a ported
behaviour on correlation is what this campaign exists to refuse.

## The next read, named precisely

`0x822A23D8` in the Xenon corpus. That turns the strongest correlation this
campaign has produced into a read fact, and it decides both the player's spawn
and whether `initial_world_position` is placing 95 units at their destinations.

## Also flagged, not chased

The player's `ScenarioObjScalars` are `(0, 30720, 0)` and unit 9's are
`(0, 6000, 0)`; 6000 also appears at `+0x28` of unit 9's tag-2 block beside 600 at
`+0x24`. Those read as ceiling and speed limits, which sits awkwardly with cycle
1142's claim that the triple is an initial position — a claim cycle 1145 already
qualified for other reasons.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
all four gates                                      ->  pass
```

No product code changed.
