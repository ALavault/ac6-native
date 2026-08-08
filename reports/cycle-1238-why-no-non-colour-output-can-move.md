# Cycle 1238 — why no non-colour output can move, proved rather than diffed

Cycle 1237 named one thing outstanding and said the next cycle should close it in
one command: whether `wireframe.png`, `depth-preview.png` and `object-id.png` are
bit-identical after the UV fix. Closed — by reading the writer rather than
regenerating, which is the stronger result.

## The proof

`u` and `v` enter `write_projected_fragment` and reach exactly one place:

```cpp
if (material.depth_test && depth_value >= depth_[pixel]) return false;
++metrics.depth_pass_fragments;
const std::uint32_t shaded = shade_fragment(context, salt, u, v);
...
color_[pixel] = shaded;                       // and the two blend arms
```

**The depth test happens before shading, and `depth_pass_fragments` is
incremented before `shade_fragment` is called.** There is no alpha test, no
colour key, no discard — nothing downstream of the texture sample can reject a
fragment. `shade_fragment`'s result is written to `color_` and to nothing else.

So the set of fragments that pass, the depth buffer, the object-id buffer and
every metric are decided by **geometry and depth alone**. A UV offset cannot
reach them.

## Why this beats the diff cycle 1237 asked for

A regeneration would have shown that the images I happened to regenerate matched.
This shows **why none of them can differ** — which covers the seven bundles at
once, including the six I would not have regenerated, and covers any future
capture too.

It also settles the claim cycle 1236 made on reasoning alone ("edges, depth and
object IDs do not sample textures") and cycle 1237 correctly refused to accept as
established. It is now established, from the writer.

## What this closes

The UV thread, opened in cycle 1233, is finished:

- the offsets are derived from the renderer's own declaration tables (1233);
- the product is corrected (1233);
- a control that would have caught it is in the test suite and scores 99.8%
  against 0.0% for both rivals the product used (1234's predecessor, the control
  commit);
- the affected artefacts are enumerated — seven bundles, not one (1237);
- and the unaffected outputs are now proved unaffected rather than assumed (here).

**Nothing in this thread remains named-but-not-done**, which is the state cycles
1227, 1236 and 1237 each had to correct in their predecessor.

## Not established, stated plainly

- `shade_fragment` itself. I read what consumes its output, not how it samples.
  Whether it decodes a texel correctly is a separate question, and cycle 1233
  already recorded that the TEXCOORD `Type` codes are unread — the corpus control
  says the values are plausible floats, not that the sampler is right.
- The seven bundles' colour images are still stale and still not regenerated,
  deliberately, for the reason recorded in the captures README.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
write_projected_fragment read in full; u,v reach shade_fragment and color_ only
```

No product code changed.
