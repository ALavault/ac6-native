# Mission 01 native recapture — current main

Post-merge native Linux recapture of the filled raster path at 1280x720. The
run uses the qualified external manifest and a deterministic 1,800-tick replay;
the headless harness advances all fixed ticks, then performs the final native
readback and replay/save/pause checks. Raw PPM, object-ID PPM, depth F32 and the
replay remain external under `/tmp/ac6-native-evidence`.

Measured native result:

- `diagnostic_point_writes=0`;
- `filled_fragment_writes=822161`;
- color coverage `361984`, depth coverage `361267`;
- terrain: `257545` unique pixels, bbox `[0,366]..[1279,718]`;
- player `f16`: `42722` object-ID pixels, bbox `[452,240]..[1113,457]`;
- player depth-pass/color writes `79140`;
- depth range `0.00241196877..0.0550876558` across visible draw records;
- `deterministic_replay=true`, `pause_stable=true`,
  `save_resume_stable=true`, `restart_stable=true`;
- native retail radio key `15` is visible in the session artifact.

`object-id.png` is the native stable-drawable-ID readback. `wireframe.png` is a
display-only edge preview derived from that readback; `depth-preview.png` is a
display-only grayscale view of the native `[0,1]` depth buffer. The product path
uses filled fragments and does not use these previews at runtime.

Conclusion: raster fill qualified; raster fill still broken for the full retail
content path until retail material/texture ownership is qualified; topology is
the next boundary; camera/clipping is the next boundary. This recapture does
not by itself qualify Mission 01 retail objectives or J1.

## Provenance

| artifact | SHA-256 |
| --- | --- |
| `color.png` | `0f4c18cf506a5e15601d50312c463062426ad9cdd9e54d86e6b244ed6652ced2` |
| `depth-preview.png` | `c60764ff7aec0c6889d5ae4045fea248f4c2ce28cbadfaf905ffb3dfddf9d209` |
| `wireframe.png` | `0621e30e5f2a8111e88220c9a213b4192d64ab7fa801e84cebe410dfcc2f9a2a7` |
| `object-id.png` | `5c5bbd45a86873f9338aa9b2144ce1722c96590e66836d2fe9e80239c3308d34` |
| `capture-metrics.json` | `ac67cd27b11555cc763d1f8601f866a72946014566b8b197dcc424ce3318397d` |
| `native-session.json` | `9175052f0505ab8943ed659e492c5228262e0d57fb59a37d4891c40c0598f77b` |
| external `manifest.tsv` | `ddec953d1ae9b21d930f86fddedb71a73ff617de7e812a72e959b476b24e54bc` |
| external replay | `5165237be95484cae21cf5a8b5d8166f5fc66c21e6b60670ddd510828cd734d4` |
| external color PPM | `0bbc4c21c11b53face2e1f8aa04a0bdf0d1c14e164522b488af252c9e33e7207` |
| external object-ID PPM | `31972b8e54863b23515226c8eebbb81c6fad3d38d14e389d982a9c88d3ddfbc7` |
| external depth F32 | `724dd5994f64c78bf973ba8d05734eed1eb06b94f26685e7b88eb0a331ebdb8e` |
| native binary | `5af7fe11d18311b1f0ca74f5497bb6319cc145f835316e7ea9c6e7f70a0087ec` |
| repo commit | `ce457c5baa3e6c62e8a87516a897aed47b69e4c6` |
