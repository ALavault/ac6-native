# Cycle 1585 — M01 campaign-backed native session

Status: `provisional-covered` for the M01-D campaign/save/replay slice;
JF remains passed, JV/JP remain open, and no promotion gate is closed here.

## Runtime boundary

Store-backed `RetailSession` now creates a PAL campaign route from the sealed
content identity: selector `1`, DPL resource `9`, DATA.TBL entry `9`, with the
Mission 01 world entry `119` when that resource is present in the cache. The
two resource identities are carried as `AssetRecord`s using their payload
SHA-256 and expanded size; no PAC bytes, tracker/tracking/telemetry files, or
generated code are copied into the repository.

The parsed sub-mission count becomes the campaign objective count. The session
transitions the selected mission through `Available -> Briefing -> Active`,
passes the campaign boundary into `MissionExecution`, and script cursor
transitions complete the corresponding campaign objective. Script exhaustion
therefore reaches the normal campaign-aware debrief rather than bypassing the
campaign state machine. Payload-only `ExternalProbe` sessions remain the
diagnostic path and do not manufacture campaign/resource identities.

Campaign records are included in product saves and in the replay semantic
digest. Existing save reads with an empty campaign section remain compatible;
non-empty campaign records are restored and validated before the checkpoint.

## Validation

* `cmake --build reconstruction/ace-combat-6/build -j16` targets
  `ac6-retail-session-tests`, `ac6-native`, and replay cadence — PASS.
* PAL payload-only session test — PASS.
* Store-backed fixture with a scenario-only cache — PASS; verifies campaign
  active, `mission_ready`, checkpoint restore and six-tick completion.
* Full qualified PAL cache corpus (missions 1–15) — PASS; all sessions open,
  and M01 qualified runtime reaches `Complete/Success` in six ticks.
* CTest product/combat/save/replay/replay-cadence — PASS (5/5).

## Remaining boundary

The frontend controller is not yet bound to this store-backed session, and the
renderer still has the documented offscreen Vulkan/readback provisional path.
Radio/XMA, frontend title→briefing→hangar rendering, and direct swapchain
DrawPacket submission remain open M01-D/E and JV/JP work.
