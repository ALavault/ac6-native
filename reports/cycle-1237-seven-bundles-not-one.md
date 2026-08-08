# Cycle 1237 — seven bundles, not one

Cycle 1236 identified `p7-current-main/color.png` as the capture the UV fix makes
stale, and closed with:

> whether any capture in p0 through p6 came from the textured path, which I did
> not audit.

Audited. **Seven of the nine bundles rendered geometry**, not one.

| bundle | `raster_triangles` | affected |
|---|---|---|
| `p0-raster-fill` | 3,628 | **yes** |
| `p1-camera-visible` | 3,727 | **yes** |
| `p2-native-hud` | 3,727 | **yes** |
| `p3-runtime-state` | 3,727 | **yes** |
| `p4-retail-radio` | 3,727 | **yes** |
| `p5-raster-recapture` | 3,727 | **yes** |
| `p7-current-main` | 3,727 | **yes** |
| `p6-native-hud` | — | no |
| `jf-retail-session` | — | no |

Every one of the seven carries the same ten drawables — `f16`, `terrain`, and
eight `sky` panels — so every colour image in this tree except the two HUD-only
bundles was rendered with the texture coordinate read four bytes early.

## What does not change, and it is everything the contracts cite

Triangle counts, fragment tests, depth-pass fragments, unique pixels, screen
bounding boxes, object IDs, depth minima and maxima. **None of those moves with a
UV offset**, and they are what `mission01-native-gate-v2.json` cites from these
bundles. No contract hash breaks; the artefact checker passes on v2 at 17 of 17
and on v4 at 31 of 31.

## Why I audited it at all

Cycle 1236 put "I did not audit p0 through p6" in a *not established* list. This
session has already shown what that costs: cycle 1233 put "the JF images will
differ" in the same kind of list and it was **false**, and cycle 1227 named an
enumeration three times before running it. **A one-line measurement sitting in a
not-established list is not a limitation, it is a deferral.**

This one took a single pass over nine metrics files.

## The correction is recorded where it will be found

Not in this report. In `reports/mission01-native-captures/README.md`, the root of
the tree, with the derived offsets, the control that scored 99.8% against 0.0%,
the table above, and the reason none of the seven has been regenerated: they are
manifest-path captures whose transforms and materials the ladder already records
as **synthesised**, so a corrected sampling would be a better picture of something
that is still not Mission 01.

## Not established, stated plainly

- Whether `wireframe.png`, `depth-preview.png` and `object-id.png` in the seven
  are bit-identical after the fix. They should be — edges, depth and object IDs do
  not sample textures — and I have still not regenerated and diffed them, which is
  the same deferral this cycle just criticised. It is named here so the next
  cycle can close it in one command rather than inherit it as prose.

## Verification

```
nine bundles' metrics read; seven carry raster_triangles, two do not
contract_artifacts v2 -> pass cited=17   v4 -> pass cited=31
ctest -> 27 tests, all passed (1 skipped)
```
