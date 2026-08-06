# Mission 01 native raster recapture — P5

This is an independent post-merge replay of the product raster path. It uses
the same 1280x720 Mission 01 manifest and replay as P1, and runs exactly 1,800
fixed ticks with `ac6-native --play-headless`. The native output was generated
from the current `main` build; no Xenia, RexGlue, XEX, Wine, or PAC runtime
dependency is involved.

The command was run with `SDL_AUDIODRIVER=dummy` under Xvfb. The native run
completed its replay, pause, save/resume and restart checks. Display-only PNGs
were derived from the native color/object-ID/depth readbacks; the product path
used filled fragments throughout.

## Measured result

- `diagnostic_point_writes=0`;
- `filled_fragment_writes=822161`;
- final color coverage `361984` pixels;
- final depth coverage `361267` pixels;
- terrain: `257545` unique pixels, bbox `[0,366]..[1279,718]`;
- player `f16`: `42722` final object-ID pixels, bbox `[452,240]..[1113,457]`;
- player depth-pass/color writes: `79140`;
- player depth range `0.00241196877..0.00522737764`;
- `deterministic_replay=true`, `pause_stable=true`,
  `save_resume_stable=true`, `restart_stable=true`;
- semantic hash `db6cfa8c0aff25f3`.

## P0 comparison

The committed P0 point-cloud baseline had `diagnostic_point_writes=0` but
terrain bbox `[0,0]..[1279,718]`, no final player object-ID attribution, and
semantic hash `1cd250de1c0a3e23`. P5 has filled interior fragments and a stable
player object-ID region. The difference is therefore measured in attribution
and geometry placement, not inferred from non-zero draw counters.

| metric | P0 baseline | P5 recapture |
|---|---:|---:|
| filled fragment writes | 1,365,250 | 822,161 |
| color coverage | 780,389 | 361,984 |
| depth coverage | 780,389 | 361,267 |
| diagnostic point writes | 0 | 0 |
| player final object-ID pixels | 0 | 42,722 |
| terrain bbox | `[0,0]..[1279,718]` | `[0,366]..[1279,718]` |

## Provenance

| artifact | SHA-256 |
|---|---|
| `color.png` | `aab3d798b87eb4fc7da36567b21c294509c23bf1cbb2ec091da5630692a9e6ff` |
| `depth-preview.png` | `5c43d18f1e91e07e7205d1bec3e90785207a32b00f6352a947ba4875d6619638` |
| `wireframe.png` | `490882174e685970d02f854ea73f387d0f0cbe141b5b67babd0361f8e1eece3a` |
| `object-id.png` | `cf7589445dbc949af29b47a2fcb9bac60346c7c876997b852fb907050963ff6a` |
| `capture-metrics.json` | `7f02134a8c95ad391995c86f478eb40f80909aff204243af242a7250f72c4a3f` |
| `native-session.json` | `0b509194669eccd022c6e27637b44f3d1df98d383a49ad037b45a63cb47ffeae` |
| external `manifest.tsv` | `2e552b538df4e36ec0a800f7c21d8f2fd17e46b5dc7b6d0449da11d7f61dc8fd` |
| external replay | `5165237be95484cae21cf5a8b5d8166f5fc66c21e6b60670ddd510828cd734d4` |
| external color PPM | `d08fb74aa4a08c409866e1d0541dcf6bd65cd3574fe977b7f1ffdc43f59f5298` |
| external object-ID PPM | `31972b8e54863b23515226c8eebbb81c6fad3d38d14e389d982a9c88d3ddfbc7` |
| external depth f32 | `724dd5994f64c78bf973ba8d05734eed1eb06b94f26685e7b88eb0a331ebdb8e` |

## Required conclusion

- raster fill qualified;
- raster fill still broken for the full retail content path until retail
  material/texture ownership is qualified;
- topology is next boundary;
- camera/clipping is next boundary.

The next product work remains bounded topology/camera and then the retail
unit/wave/objective boundary. Textures and HUD semantics are not promoted by
this capture.
