# Mission 01 native P1 camera/visibility capture

Exact native Linux replay after the aircraft world-frame anchor and the
explicit scene transform were applied:

- 1,800 fixed ticks;
- 1280x720;
- replay `/tmp/ac6-native-evidence/mission01.replay`;
- runtime manifest `/tmp/ac6-mission01-native-p1-camera/manifest.tsv`;
- no Xenia, RexGlue, XEX, Wine or PAC runtime dependency.

The native renderer now applies the `WorldFrame` player position to model-local
aircraft geometry. The external manifest keeps the qualified retail slices and
sets the scene terrain transform to `(0,-40,0)`; the manifest and transform
file are content-addressed below. This is a native scene calibration, not a
claim of retail camera/transform parity.

Capture results:

- `diagnostic_point_writes=0`;
- `filled_fragment_writes=822161`;
- final color/depth coverage `361267` pixels;
- terrain: `257545` unique pixels, bbox `[0,366]..[1279,718]`;
- player `f16`: `42722` final object-ID pixels, bbox `[452,240]..[1113,457]`;
- player depth-pass/color writes: `79140`;
- depth range is non-degenerate (`0.00241196877..0.0550876558` across the
  visible terrain/player draw records);
- semantic replay hash `84a39be4daf4e71f`;
- `deterministic_replay=true`, `pause_stable=true`,
  `save_resume_stable=true`.

`object-id.png` is the native stable-drawable-ID readback. `depth-preview.png`
and `wireframe.png` are display-only derivatives; the product path remains the
filled raster path.

## Provenance hashes

| artifact | SHA-256 |
| --- | --- |
| `color.png` | `d2016e0fa5bbd0c7b590b24e91e7e366c3fb8692fc1d01a6a98fd5f3bcdbdefe` |
| `depth-preview.png` | `c88d4a2509c092e004237562c39c0319c2e5e438784c898640779300dcbbe05f` |
| `wireframe.png` | `9b67ab74799f5f778e6b18f495eb9c0c34071f1568d15dde2e27b27c324364a7` |
| `object-id.png` | `22e2d244cbb412c6537b541f63984a2ec017601271fbad7c67c31544449030e6` |
| `capture-metrics.json` | `62cf04295349d31cf59535aa7923be9b88ac590fe3697334d503e6346b242dcd` |
| `native-session.json` | `5a2bfdf10a9b1a533b7b5a461f0309f40847a48e86682d564c5a5ea41f7c48b6` |
| external `manifest.tsv` | `2e552b538df4e36ec0a800f7c21d8f2fd17e46b5dc7b6d0449da11d7f61dc8fd` |
| external `transforms.tsv` | `fc4ed417367b2711e7bb8d08f2b813ba55f1089c2ef7ead5e7216828c74de0fb` |

This capture closes the native `world_visible` and
`player_aircraft_visible` evidence conditions. It does not close Mission 01
retail objectives, waves, HUD, radio, debrief or J1.
