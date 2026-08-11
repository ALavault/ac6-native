# Checkpoint 2 — mission bundle boundary

`RetailMissionBundle` is now the product-facing mission constructor. It accepts
the mission identifier, difficulty, and validated `CampaignLoadout`, resolves
the corresponding PAL DATA.TBL campaign payload from `RetailContentStore`, and
retains the cache identity with the payload. The old direct payload overload is
still present only for parser/runtime tests.

The retail session opens this bundle before parsing the scenario. Invalid
mission IDs, unresolved loadouts, or out-of-range difficulties fail before a
world is constructed. Existing retail session and replay tests pass unchanged.
