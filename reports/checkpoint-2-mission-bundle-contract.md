# Checkpoint 2 — mission bundle boundary

`RetailMissionBundle` is now the product-facing mission constructor. It accepts
the mission identifier, difficulty, and validated `CampaignLoadout`, resolves
the corresponding PAL DATA.TBL campaign payload from `RetailContentStore`, and
retains the cache identity with the payload. The old direct payload overload is
still present only for parser/runtime tests.

The bundle now qualifies child 0 at open time with the shared
`ScenarioPayload`/`MissionScenario` readers and retains both parsed views. The
store-backed session moves those qualified views into its runtime, so an
accepted product launch does not re-open an arbitrary child payload. Invalid
mission IDs, unresolved loadouts, malformed scenarios, or out-of-range
difficulties fail before a world is constructed. The direct payload overload
still parses independently and remains limited to parser/runtime tests.

The qualified-cache session test now asserts that the bundle retained parsed
views for all 15 campaign payloads and checks the shared readers against the
same bytes before the Mission 07 condition assertions. The PAL cache identity
used by this corpus is
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.

The same qualified run now constructs a `RetailSession` and executes one fixed
1/60-second tick for every mission id 1–15. This is a bootstrap guard only: it
proves common bundle/world/camera construction, not mission objectives, AI,
render parity, or completion semantics.
