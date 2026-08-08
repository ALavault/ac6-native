# Cycle 1177 — PLAD is exhausted, and the player's absence is one pattern

## The negative, tightened to exhaustive

Cycle 1146 found that the three callers of the `PLAD` record getter
`0x82249BC8` read only word 3 — the route cursor — and none reads the record's
three floats. That was a statement about the callers it had found.

Both accessors, every call site in the image:

```
0x82249BA8  bind    0x82097FB8   0x8219C830   0x821A0318
0x82249BC8  get     0x82097FC8   0x8219C840   0x821A0328
```

Three bind/get pairs, adjacent in three functions, and cycle 1146 read all three
getters. The container is a stack local in each, bound and read once. **There is
no fourth path**, so PLAD's floats have no consumer in the image through this
API, and the negative is now exhaustive rather than bounded.

Mission 1's PLAD triple `(-2025, 1500, 1345)` remains the most player-spawn-shaped
value in the archive and remains unread.

## The player is absent from three tables, not one

Cycle 1176 found classes 0 and 4 carry no model index on any record, class 0
being the player. Put beside what was already known:

| what everything else has | where the player is |
|---|---|
| a tag-2 order giving a load-time position (cycle 1145) | has none |
| a model index at `+0x61` (cycle 1176) | has none |
| — | `PLAD`'s floats, which nothing reads (this cycle) |

Two of those were logged as separate open questions. They are one: **the mission
does not describe the player's aircraft or where it starts.**

That is not a gap in the data, it is a fact about the game. The player chooses an
aircraft before the mission, so the mission cannot name it; and a spawn that
depends on that choice does not belong in the mission's unit table either. The
scenario container describes the world the player is dropped into, not the
player.

This is a hypothesis about design, and it is labelled as one. What it changes is
where to look: not for a missing field in the container — three separate searches
have now come back empty — but for the code that builds the player object from
whatever the hangar left behind.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
