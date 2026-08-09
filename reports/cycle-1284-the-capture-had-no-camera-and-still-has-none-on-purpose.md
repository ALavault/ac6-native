# Cycle 1284 — the capture had no camera, and still has none, on purpose

## Qualification

`default.xex` SHA-256 `acc302c1…11bcde`; payload
`reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin`.
**No oracle pass was spent.**

## Established — there was never a camera

`WorldFrame`'s camera fields are `float camera_x{}` … `camera_target_z{}`,
zero-initialised, and **nothing on the retail path writes them**. So the capture
cycle 1269 fused was rendered with the eye at the origin and the target at the
origin — a degenerate camera — and its **29 of 95 markers are whatever that
catches** of a 66,456-unit world.

Cycle 1269 wrote that "29 of 95 is a statement about the camera". It is more
exact than that: it is a statement about *no camera*, and I did not check which
before writing the sentence.

## The attempt, and what it measured

Cycle 1283 derived retail's shape — `camPos = playerPos + R_player · offset`,
basis from the player's own locator rows — and established that the **offset and
FOV are not in the XEX**: they are fields of a 144-byte record copied each frame
from a runtime per-aircraft table, of which the 46° initialiser is only the
fallback.

So the shape is derivable and the numbers are not. I placed the eye behind and
above the player with a **chosen** offset, labelled as chosen.

**It drew 0 of 95.**

The reason is not the camera. The player's position reads
`(-1.1368e-26, 4.19661e-41, 0)` — denormal noise, never written — because the
class-0 Set is one of the **135 units the container gives no load-time
position**, and the placement push runs at first update from an FSM that
construction installs dead (cycle 1279). The camera was following a unit that
has no place to be.

## The change that shipped

`render_world_markers` has refused to draw an unplaced unit since it was
written: *"a unit the container gives no load-time position is not drawn at all.
The origin is not a fallback, it is a different claim."* **The camera now owes
the same refusal**, and takes it: it follows the player only if the player is in
`world().placed`, which it is not, and the artefact records why.

```
world_markers_drawn                                  29
world_units_placed                                   95
camera_follows_player                                False
camera_refused_player_has_no_load_time_position      True
```

The count is unchanged. What changed is that the capture now **states** it has
no camera and gives the reason, instead of leaving a reader to infer intent from
a picture.

## Why this is the right outcome rather than a failure

Two wrong ways to reach a higher number were available. One was to keep the
player-following camera and let it render from noise — 0 markers, a black frame,
and a `camera_follows_player: true` that means nothing. The other was to point
the camera at the world's centroid, which would have drawn most of 95 and looked
like progress while measuring the framing I chose rather than anything retail
does.

**The marker count cannot honestly improve until the placement runs**, because
until then the player has nowhere to stand. That is now one line in the artefact
rather than a conclusion a reader has to reconstruct.

## Not established

- Whether any of the 95 placed units is a sensible camera anchor. Not tried:
  anchoring on a non-player unit would measure a choice, not retail.
- The chosen offset — `0.02 × extent` back, `0.008 × extent` up — is arbitrary
  and is marked `camera_offset_is_chosen_not_derived` in the report. It has
  never been exercised, because the refusal fires first.
