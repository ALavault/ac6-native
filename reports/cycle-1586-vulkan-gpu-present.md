# Cycle 1586 — Vulkan GPU-only interactive presentation

Status: `provisional-covered` for the JV presentation transport; JV/JP remain
open because the scene still submits only the qualified provisional Mission 01
draw and does not yet carry the complete world/HUD/TCAM cone.

## Boundary

When `play` runs without `--capture` or `--scene-capture`, `NativeGraphics`
creates the SDL-qualified instance and surface, then `VulkanBackend` selects a
present-capable device on that same instance. The scene renderer renders its
native Vulkan target and calls `present_target`: a GPU image blit transitions
the target and acquired swapchain image, then presents with `vkQueuePresentKHR`.
There is no CPU readback, RGBA staging upload, or `NativeRenderTarget` in this
interactive path. Capture modes retain the diagnostic offscreen/readback path
so PPM artefacts remain reproducible.

The backend does not destroy the caller-owned SDL instance/surface. Its
swapchain images, views and acquire fence are destroyed before the device; the
platform layer destroys the instance after the backend has gone out of scope.

## Validation

* Full incremental `cmake --build reconstruction/ace-combat-6/build -j16` —
  PASS.
* CTest SDL input, replay cadence, Vulkan backend, scene renderer and resource
  cache — PASS (6/6).
* `SDL_AUDIODRIVER=dummy xvfb-run -a ac6-native play --cache
  ac6-native-cache-cp3-20260811 --frames 1` — PASS:
  `ticks=2 replay_frames=1`, PAL cache index
  `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.

No retail container, tracker/tracking/telemetry file, generated C++, or CPU
raster output was added.

## Remaining qualification

The current scene uses one bounded clip-textured map primitive and a provisional
fit matrix; direct DrawPacket submission for all 4,226 placements, terrain
depth/materials, camera TCAM and GPU HUD remain to be qualified before JV/JP can
close.
