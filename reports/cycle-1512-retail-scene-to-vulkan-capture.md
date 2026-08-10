# Cycle 1512 — retail scene bytes reach the Vulkan target

## Delivered

- Added `NativeRenderTarget::blit_argb32`, a bounded CPU-reference bridge that
  copies 0xAARRGGBB pixels and normalises the retail camera-space depth plane
  into the existing target. It resets diagnostic/object-id state so the Vulkan
  presenter consumes exactly the imported frame.
- Added the explicit `ac6-native play --scene-capture PPM [--scene-report JSON]`
  lane. It opens `RetailMission01CpuCompositor` from the sealed cache, uses the
  qualified mode-2 base offset with a fixed inspection pose, overlays the live
  HUD, and presents/captures the result through the normal Vulkan presenter.
  The live player pose/camera producers remain fail-closed and this lane never
  claims JV.
- Added a raster-target regression test for colour/depth import and dimensions.

## Validation

Against cache index SHA-256
`ca25a10fc9dbf1987d34fd755c9d842c887273e5083e8b4d644339b5a156bbde`:

```
SDL_AUDIODRIVER=dummy xvfb-run -a ac6-native play \
  --cache /tmp/ac6-native-cycle1511-fonts \
  --scene-capture /tmp/ac6-scene-blit.ppm \
  --scene-report /tmp/ac6-scene-blit.json --frames 0
ac6_retail=pass command=play mission=1 ticks=2 replay_frames=1
```

The 1280×720 report records `terrain_instances_rasterized=941`,
`city_instances_rasterized=1620`, `water_fragment_writes=2002`,
`color_coverage=444296`, `depth_coverage=444296`, `marker_free=true` and
`jv_eligible=false`. The presented PPM SHA-256 is
`1936577031a129b7085e2104609c2036ea9fe7dca5722f61d187286842bb9690`.

The native raster test and the complete SDL-audio dummy CTest corpus pass:
`100% tests passed, 0 tests failed out of 69` (one intentionally skipped
retail-font test without its opt-in cache environment).

## Boundary retained

This materially improves the screencap and proves cache-backed scene bytes are
presented by the Vulkan path. It does not close live pose/camera, sky,
vegetation, active-unit geometry, gameplay progression or the JV/JP gates.
