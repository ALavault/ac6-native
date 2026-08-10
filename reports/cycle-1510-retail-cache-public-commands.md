# Cycle 1510 — public retail play/replay boundary

## Delivered

- Added `ac6-native play --cache CACHE_ROOT [--save SAVE_PATH]` with a
  persistent SDL/Vulkan window, fixed 60 Hz ticks, controller/keyboard input,
  optional atomic session save, and optional identity-sealed replay capture.
- Added `ac6-native replay --cache CACHE_ROOT --replay FILE --report OUTPUT_DIR`.
  The replay format records the mission, aircraft, weapon, capability bit and
  retail content-index SHA-256.  A replay from another cache is rejected before
  a session is opened.
- Added `AC6RTPLY` version 1 with bounded frame count, strict truncation/trailing
  byte checks, atomic writes, and fail-closed invalid identity/loadout handling.
- Product commands require the imported common camera table and qualify the
  aircraft ordinal against all 15 retail camera groups.  The bounded
  payload-only/session fixtures remain available for parser tests.

## Evidence

- `ac6-retail-session-replay-tests`: round-trip, bad magic, missing identity,
  and unqualified loadout checks pass.
- PAL cache replay:

  ```text
  ac6_retail=pass command=replay mission=1 frames=4 deterministic=true
  semantic_hash=0xa0125b5d0720e787
  ```

- Replay report records `forced_progression=false`, `simulation_hz=60`, and
  `cache_index_sha256=349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
- A replay with one altered identity byte exits with
  `error=cache_digest_mismatch`; aircraft 16 exits with
  `error=cache_incomplete detail=loadout_capability_table`.
- Full CTest: 68/68. Python tooling tests: 87/87. Retail cache audit:
  15 missions, 17 blobs.

## Boundary kept explicit

The command now reaches the real cache-backed `RetailSession` and records live
input, but the remaining live retail pose/mission guard and the full retail
geometry-to-session compositor are still open.  This cycle therefore does not
claim JV or JP; it removes a product/API gap without introducing synthetic
progression or a fallback camera.
