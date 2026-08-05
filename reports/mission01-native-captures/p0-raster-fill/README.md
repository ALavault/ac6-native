# Mission 01 native P0 raster-fill capture

This directory is the exact native Linux replay capture after the P0 filled
rasterizer fix:

- 1,800 fixed ticks;
- the existing `/tmp/ac6-native-evidence/mission01.replay`;
- 1280x720;
- qualified external slices only;
- no Xenia, RexGlue, XEX, PAC or replay renderer at runtime.

The native product path records `diagnostic_point_writes=0` and
`filled_fragment_writes=1365250`. The final readback has 780389 non-clear color
pixels and 780389 non-clear depth pixels. The terrain drawable records 6765275
inside fragments, 1344349 depth-pass/color writes, 382 non-degenerate clipped
triangles and a screen bbox from `(0,0)` to `(1279,718)`.

`object-id.png` is a native stable-drawable-ID readback. Its final non-clear
pixels are attributed to `terrain`; no final pixels are attributed to the
player `f16` drawable. `wireframe.png` is a display-only edge preview derived
from the filled color readback, not a product wireframe pass. `depth-preview.png`
is a display-only inverted view of the native `[0,1]` depth buffer.

The replay is deterministic (`semantic_hash=1cd250de1c0a3e23`), pause and
save/resume are stable. These captures do not pass `world_visible` or
`player_aircraft_visible`: the fill is real, but the terrain is oversized and
collated against the viewport and the player is fully occluded in the final
object-ID readback.

## SHA-256

| file | SHA-256 |
| --- | --- |
| `color.png` | `3a793d3428966b0ae16d843f6a2d35e0495602a3934cc63775874f2ede2daa5f` |
| `depth-preview.png` | `97eef1a78f857cb3804942052409c493c6b8398103e24838e5efc5f2a5f92798` |
| `wireframe.png` | `8fe0bb6c0f832300e9c22f327f41c3342dfdf517983b73288a9061b1726270677` |
| `object-id.png` | `e4c01e4499feca17ee3415109702835aa713282fa4ea0eabfe785b7c51037b63` |
| `capture-metrics.json` | `87281465b47b8d36eac23be0208e7b46d0c2275dcb4cb4305d2a9de89eb9adcf` |
| `native-session.json` | `ecb7605e92609942e2eed53fba4b93496710feaa37eddd3bbc8f6c2b60095fd0` |

Conclusion: raster fill qualified; camera/clipping is the next boundary.
Topology is covered by the deterministic strip/restart tests and is not
re-opened without contradictory evidence.
