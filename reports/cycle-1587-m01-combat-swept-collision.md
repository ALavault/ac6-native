# Cycle 1587 — M01 projectile trajectory and swept collision

Status: `provisional-covered` for the M01-C projectile/collision slice; the
full retail combat/AI/objective cone and JV/JP remain open.

## Change

`CombatWorld::tick` now tests each projectile against the target over the
whole fixed-tick segment instead of only at the new endpoint. This preserves
the deterministic fixed-step state while preventing a fast round from
tunnelling through a PAL target between two frames. A bounded Mission 01
session test locks the nearest hostile only on X, fires only on the A rising
edge, waits at the read-only `ExternalProbe` scheduler boundary, and verifies
that the payload-provided target receives one damage event and becomes
inactive. No target, health, position, or collision is synthesized by the
test.

## Validation

* `ac6-retail-session-tests` with the PAL Mission 01 payload — PASS.
* `ac6-combat-runtime-tests` — PASS; the generic 20-unit crossing also
  resolves automatically and records one damage event.
* No PAC/container bytes, tracker/tracking/telemetry files, generated C++, or
  CPU raster assets were added.

## Remaining qualification

The product `QualifiedRuntime` scheduler still advances the six parsed M01
steps before this ~2 km projectile can arrive, so this report deliberately
does not promote the combat slice to retail parity. Wave/AI producers,
weapon tables, target/objective coupling, and a qualified in-mission cadence
remain M01-C work.
