# Cycle 1146 — the far plane was eating the world, and cycle 1145 blamed the camera

## The correction, first

Cycle 1145 wrote that only four of the 95 placed units reach the screen because
"the camera is still the rasteriser's hardcoded fallback, sitting at the origin"
while the units are thousands of units away. The distance is real. The mechanism
was wrong, and I did not measure it before writing it down.

`project_point` ends with:

```cpp
constexpr float far_plane = 4096.0f;
screen.depth = std::clamp(view_z / far_plane, 0.0f, 1.0f);
```

Every point beyond 4,096 world units normalises to **exactly 1.0**. The render
target is cleared to depth 1.0. `draw_world_marker` tests
`if (depth_value >= depth_[pixel]) continue;`. So every marker beyond 4,096
units was rejected — not clipped by a frustum, not missed by a camera, but
silently dropped by a depth contract meeting a clear value.

Mission 01 spans 66,000 world units on x. The far plane is 4,096. Sixteen
mission-widths of the world were invisible for a reason that had nothing to do
with where the camera pointed.

This was found by building the overview plot and watching it draw **zero**
markers with the camera placed exactly right, looking straight at the centroid
of the bounding box it had just computed. A camera that frames everything and
draws nothing is not a camera problem.

## The fix

`project_point` takes a `far_plane_override`, defaulting to the historic 4096,
and `draw_world_marker` and `RetailSession::render_world_markers` thread it. The
geometry path and the live view are unchanged: 4,096 is right for content near
the player. A caller plotting a whole mission passes the extent it measured.

## The overview plot

`reports/mission01-native-captures/jf-retail-session/world-overview.png`.

The camera is **chosen, not derived** — placed from the bounding box of the
derived positions, looking down at their centroid. That is stated in the source,
in the metrics JSON (`overview_camera_is_chosen_not_derived: true`) and in the
capture README, and it is legitimate only because the marker lane was already
declared diagnostic: no material, no texture, no topology, no parity claim. A
capture that framed itself this way while presenting as the retail camera would
be worthless.

What it shows is a mission map. The factions occupy distinct regions, with one
isolated group east of the main engagement. That is the first time this
reconstruction has produced an image whose *content* can be argued with.

## Three numbers, and neither gap is a defect

```
units placed                   95
distinct spawn coordinates     59
markers on the overview plot   57
```

**95 → 59.** Retail spawns a formation's members at one point. Their per-member
separation is the Obj triple — and that triple needs the parent frame cycle 1145
showed is never assigned. The same debt, surfacing again, this time as markers
sitting on top of markers. It is a good sign, not a bad one: it says the two
halves of the placement really are the two halves they looked like.

**59 → 57.** At this zoom two distinct positions round onto a pixel another
marker already wrote.

Both are asserted exactly in `retail_playable_tests.cpp` and
`retail_session_tests.cpp` before any image is written, because a plot whose
count drifts has stopped being evidence.

## Two negative results

**The container does not hold the player's spawn.** The payload has ten root
slots; the parser consumes 0, 1, 2, 5 and 6. I dumped the other five. Slot 3 is
54 records of 16-bit indices with no floats, slot 4 and slot 6 are empty, slot 7
is three records of small scalars (1.0, 5.0, 8.0, 10.0), slot 8 is two entries
headed `12000 / 800` whose record lists are all-zero templates, and slot 9 is
five records of small scalars. **No unconsumed slot contains a float triple in
world range.** The player's start is not in this container.

**The AC6_recomp bridge is not in this workspace.** The plan I wrote this
session lists it under "Verified state" as "built, boots to airborne Mission 01,
live input, no fatal". That entry was inherited from earlier reports and I did
not check it. There is no `AC6_recomp` binary or source tree anywhere under
`/fastdata/lavaulta`. Only `patches/` survives — `ac6_boundary_probe.cpp`, the
hook TOMLs, the boundary-probe patches.

This is the second inherited claim about workspace contents I have had to
correct in this campaign, in the opposite direction from the first: cycles
1130–1131 said the retail archives were absent when they were present, and now
the plan says the bridge is present when it is absent. The lesson generalises —
**a "verified state" table is only verified on the day someone runs `ls`.**

## What this makes of the next step

Step 2c wanted a flight camera. It is blocked on the player's spawn, which is:

- not in the player's behaviour program — unit 0 has one Obj sub-record at
  `(0,0,0)` and no tag-2 order at all;
- not in `PLAD` — all three callers of the record getter `0x82249BC8` read only
  word 3 into `+0xF0`, the route cursor (cycle 1145);
- not anywhere in the scenario container — the slot sweep above.

The remaining static lead is that route cursor, and cycle 1145 already recorded
the trap it sits behind: `grep "0xf0(r"` returns mostly vtable dispatch, so
separating field reads from virtual calls needs an instrument that tracks which
register holds the object.

**The oracle lane is a qualified blocker.** The plan authorised the oracle
freely and named the bridge as the instrument; the bridge is not here. Xenia
remains, via `scripts/launch_xenia_ac6_wine.sh`, and
`XENIA_WINE_ORACLE_HANDOFF.md` documents an interactive route with a profile and
a save — which is exactly the "may need an attended session" risk the plan
listed.

## Decided rather than asked

- **The plot's far plane is `4 * extent`, not the extent.** The extent is the
  widest axis of the bounding box; the camera sits above the centroid, so the
  furthest marker is further than the extent. Four times it is comfortably past
  every marker and still finite, so depths stay ordered and distinguishable.
- **The live and debrief captures keep the 4096 far plane.** They are the
  player's view, the player is at the origin, and changing their depth contract
  to flatter the count would be exactly the "looks right" failure the bundle
  exists to prevent. They still show four markers, and the plot explains why.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  24/24 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_class_map.py ... --require J2              ->  class_map=pass vtables=811 rejects=1619
```
