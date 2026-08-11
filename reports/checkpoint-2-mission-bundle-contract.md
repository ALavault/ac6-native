# Checkpoint 2 — mission bundle boundary

`RetailMissionBundle` is now the product-facing mission constructor. It accepts
the mission identifier, difficulty, and validated `CampaignLoadout`, resolves
the corresponding PAL DATA.TBL campaign payload from `RetailContentStore`, and
retains the cache identity with the payload. The old direct payload overload is
still present only for parser/runtime tests.

The retail session opens this bundle before parsing the scenario. Invalid
mission IDs, unresolved loadouts, or out-of-range difficulties fail before a
world is constructed. Existing retail session and replay tests pass unchanged.

The qualified-cache session test now opens the bundle and runs the shared
`ScenarioPayload`/`MissionScenario` readers for all 15 campaign payloads before
the Mission 01 runtime assertions. The PAL cache identity used by this corpus
is `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.

The same qualified run now constructs a `RetailSession` and executes one fixed
1/60-second tick for every mission id 1–15. This is a bootstrap guard only: it
proves common bundle/world/camera construction, not mission objectives, AI,
render parity, or completion semantics.
