# Cycle 1589 — M01 replay digest includes frontend state

Status: `provisional-covered` for the frontend contribution to the semantic
replay digest; the full audio/media and Vulkan HUD domains remain open.

## Change

The store-backed replay semantic hash now carries the frontend handoff flag
and `FrontendState` alongside the campaign snapshot, combat state, script
cursor and world frame. The replay report basis is versioned as
`world_script_combat_campaign_frontend_v2`; two replays of the same PAL input
stream therefore compare the title/mission/debrief boundary as well as the
simulation state.

## Validation

* Full incremental build — PASS.
* Product, combat, save, replay and cadence CTest targets — PASS (6/6 in the
  checkpoint run).
* PAL Xvfb `play --frames 1` and the complete-cache frontend session matrix —
  PASS.
* No PAC bytes, tracker/tracking/telemetry files, generated C++, or CPU raster
  assets were added.

## Remaining qualification

The digest still has no qualified radio PCM/subtitle hash or GPU DrawPacket
hash. Those domains remain open until the retail RadioTbl -> TextData ->
RIFF/XMA mapping and complete Vulkan scene/HUD are evidenced.
