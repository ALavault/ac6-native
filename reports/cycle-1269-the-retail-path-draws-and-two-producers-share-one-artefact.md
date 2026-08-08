# Cycle 1269 — the retail path draws, and two producers share one artefact

## Qualification

Payload `reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin`;
`default.xex` SHA-256 `acc302c1…11bcde`. **No oracle pass was spent.**

## Established — JV 2a, the halves are fused

`run_retail_session` rendered the HUD and nothing else: `render_world_markers`
existed on `RetailSession`, was exercised by the tests, and was never called by
the command that produces the retail capture. It is now called, before the HUD.

Measured on Mission 01:

| | |
|---|---:|
| units published | 230 |
| units the container gives a load-time position | **95** |
| markers drawn | **29** |
| world extent | **66,456** |

### The far plane is derived, not defaulted, and that is the whole of the care

```cpp
const float world_extent = std::max({max_x - min_x, max_z - min_z, 1.0f});
const std::size_t markers =
    session->render_world_markers(target, frame.world, 4.0f * world_extent);
```

Cycle 1146 paid for this once: `project_point` normalises depth as
`view_z / far`, so with the 4,096 default every marker beyond 4,096 units
saturates to 1.0, fails the depth test, and disappears. Mission 01 spans
**66,456** units. The default would have dropped almost the entire world and
left a picture that looks deliberate — a HUD over a near-empty frame, which is
exactly what the capture looked like before and would have looked like after.

**29 of 95 is a statement about the camera, not about the world.** JV 2c — the
flight camera — is open; the fallback is a 60° guess. The number is in the
report so that when 2c closes, the change is visible as a number rather than as
an impression.

## The finding — two producers write one artefact, and the last one wins

`reports/mission01-retail/retail-session.json` is written by **both**
`run_retail_session` (the CLI) and `ac6-retail-session-tests` (line 422 of the
test, via `argv[2]`). They write different schemas of the same name.

The sequence here: the CLI was run, the artefact gained
`world_markers_drawn`, `world_units_placed` and `world_extent`, the contract
hashes were refreshed against it — and then `ctest` **overwrote the file with
the test's version**, silently reverting all three fields and leaving refreshed
hashes describing content that no longer existed.

`CLAUDE.md` already carries the rule that catches this — *run `git status` after
`ctest`, not before* — written after an artefact was regenerated and never
staged. **This is a second, different failure of the same shape**: not a stale
hash over new content, but a *reverted artefact* under a fresh hash. The rule
caught it because the working tree came back clean when it should not have.

That two commands produce one contract-cited path is the defect, and it is
recorded rather than fixed: choosing which producer owns the artefact changes
what the contract is asserting, and that is a decision, not a cleanup.

## Not established

- **Whether 29 is the right number of markers for this camera.** Nothing
  asserts it; the CLI prints it and no test checks it. The test's own marker
  assertions (`live_markers > 0 && <= 230`) run on a different target with a
  different camera.
- **`src/commands.cpp` is cited by no contract**, so this change is outside every
  behaviour's evidence — the same gap `src/native_geometry_raster.cpp` has. The
  CLI entry points are not under contract at all.
- Why 95 of 230 units carry a load-time position and 135 do not. That is the
  placement chain cycle 1244 proved runs at first update, not at load, and it is
  unchanged by this cycle.
