# Cycle 1236 — which captures the UV fix actually changes, and cycle 1233 got it backwards

## The correction

Cycle 1233 fixed the UV offset and wrote, under what it had not done:

> the JF capture images will differ; its asserted numbers will not. I have not
> regenerated them.

**The JF images do not differ.** `src/retail_session.cpp` contains no reference to
`NativeGeometryDatabase`, `DecodedGeometry` or any decode path — the JF bundle
draws a HUD and one marker per unit, with no mesh, no texture and therefore no
texture coordinate. A UV offset cannot reach it.

I carried a debt that did not exist, and named it in a "not established" list,
which is the wrong place for a claim I had not checked.

## What is actually stale

`reports/mission01-native-captures/p7-current-main/color.png` — the manifest-path
capture, the one with textured geometry. It was rendered by a raster target that
sampled UV four bytes early on every vertex, so it is **wrong in a known way and
committed**.

Its siblings in that bundle are not:

| artefact | affected by UV? | cited by |
|---|---|---|
| `capture-metrics.json` | **no** — counts: triangles, fragment writes, drawables | v2 contract |
| `native-session.json` | **no** — tick and replay determinism | v2 contract |
| `object-id.png` | **no** — object IDs per pixel, not shading | v2 contract |
| `color.png` | **yes** | no contract |
| `wireframe.png` | no — edges, not sampling | no contract |

So **no contract hash breaks**, and the artefact checker still passes on v2. The
stale thing is the one picture nothing cites.

## What I am doing about it, and why

**Not regenerating it.** Three reasons, and the third is the real one:

1. It is cited by nothing, so no gate is weakened by leaving it.
2. Regenerating changes a committed binary artefact for a visual difference
   nobody can currently check against an oracle — and `MISSION01_LADDER.md`
   already records that the manifest path's transforms, materials and textures
   are **synthesised**, so a corrected sampling of a synthetic scene is a better
   picture of something that is still not Mission 01.
3. A regeneration belongs with the work that makes the picture mean something —
   JV's fused retail-session render — not bolted onto a bug fix.

**It is recorded as stale in the bundle's own README** rather than left to be
discovered, which is the part that matters.

## Not established, stated plainly

- Whether `wireframe.png` and `depth-preview.png` are bit-identical after the
  fix. I reasoned that edges and depth do not sample textures; I did not
  regenerate and diff them.
- Whether any *other* committed capture in the eight bundles under
  `mission01-native-captures/` came from the textured path. I checked p7 because
  the ladder names it; I did not audit p0 through p6.

## Verification

```
contract_artifacts (v2)  ->  pass
ctest                    ->  27 tests, all passed (1 skipped)
audit --require JF       ->  mission01_final_gate=audit-valid JF=pass open=none
retail_session.cpp: zero references to the geometry decode path
```
