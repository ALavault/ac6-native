# Cycle 1588 — M01 PAL frontend handoff

Status: `provisional-covered` for the frontend state boundary; the Vulkan
frontend/HUD draw path and full M01 mission objective/media cone remain open.

## Change

When the sealed PAL cache contains the complete font/glyph closure, the
store-backed `RetailSession` now owns a `FrontendController` handoff. It
configures English/Normal/Normal, selects Mission 01, drives Title -> New
Game -> Briefing -> Hangar -> Loading -> Mission, sets the qualified loadout
at the hangar boundary, and keeps the campaign pointer shared with the
mission execution. Completion/abort transfers the same debrief to the
frontend; pause/resume and restart keep the frontend and mission HSM in sync.

Scenario-only stores without entries 2..8 retain the bounded direct campaign
path and are not promoted as frontend evidence.

## Validation

* PAL cache session matrix for missions 1–15 — PASS; frontend enabled and in
  `Mission` for each complete-cache session.
* M01 `QualifiedRuntime` on the PAL cache — PASS; execution reaches
  `Complete/Success` and frontend reaches `Debrief`.
* `ac6-native play --cache ... --frames 1` under Xvfb with
  `SDL_AUDIODRIVER=dummy` — PASS (`ticks=2 replay_frames=1`).
* Payload-only session and scenario-only store tests — PASS; their diagnostic
  frontend fallback remains explicit.
* No PAC bytes, tracker/tracking/telemetry files, generated C++, or CPU raster
  assets were added.

## Remaining qualification

The direct Vulkan renderer still submits the provisional map clip primitive;
frontend title/briefing/hangar/loading/debrief visuals, GPU HUD, audio/XMA,
and a qualified in-mission cadence remain open M01-D/E and JV/JP work.
