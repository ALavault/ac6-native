# Cycle 524 — native readiness control

Date: 2026-08-02

Qualification is unchanged from cycle 523: PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
canonical project `ghidra-projects/ace-combat-6`, native HEAD
`6c417820fd6a89aade3cb02c53ddf4f222b81991`.

## Outcome

The exact readiness event is `207`.

A read-only canonical Ghidra dump of the 38 program-endian words at
`0x8214D3B4` maps event index `7` (`200 + 7`) to `0x8214D5B0`, the arm that
toggles `manager+35994`. The dump was produced by
`scripts/DumpQualifiedWords.java`; it self-qualified program name and the full
PAL XEX SHA-256.

Literal control flow in `sub_82146DB8` already contains the event-207 producer:

```text
state manager+35984 == 0
level_root[0x276A0] & 0x20 != 0
manager vslot +0x70 returns 0
    -> manager vslot +0x04(manager, 207)
```

The prior forced-ready profile writes status byte `manager+35995=5`, so cycle
524 repeated the complete route with both force options disabled. The result is
unchanged:

- `manager+35984=0`;
- vslot `+0x70` (`sub_82144FD8`) repeatedly returns 0;
- callback `sub_8214D390` receives only event 22 twice;
- event 207 is never delivered;
- readiness `manager+35994` remains 0.

With the other two predicates dynamically true, the canonical branch proves
that the missing gate is `level_root+0x276A0` bit `0x20`. The next question is
which qualified producer owns that bit and why the first-campaign route does
not publish it after the two loadout resources resolve.

This control also proves that the observed missing event is not an artefact of
`ac6_force_loadout_ready`.

## Event table

```text
event 200 -> 8214D44C
event 201 -> 8214D4A4
event 202 -> 8214D4F8
event 203 -> 8214D50C
event 204 -> 8214D520
event 205 -> 8214D534
event 206 -> 8214D548
event 207 -> 8214D5B0  readiness toggle
event 208..235 -> 8214D5DC no-op return
event 236 -> 8214D5E0
event 237 -> 8214D5F0
```

## Reproduction

Scenario:

- `scripts/ac6-first-mission-loadout-native-ready-probe.steps`

It uses the same qualified route as cycle 523, performs no force override,
sends no mission-launch input and observes for 12 seconds after the final
loadout `A`.

```sh
tools/ac6-run.sh \
  --out /fastdata/lavaulta/auto-re-agent/reports/logs/cycle-524-loadout-native-ready-control \
  --duration 210 --display :134 --capture-at 0 --startup-timeout 120 \
  --keys '0:Escape:0.1,2:space:0.1' --wait-for 'type28=30' \
  --wait-pulse 'Escape+space:0.1:2' \
  --step-file /fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/scripts/ac6-first-mission-loadout-native-ready-probe.steps \
  -- --ac6_log_ui_dispatch=true --ac6_log_loadout_dispatch=true \
  --ac6_force_loadout_ready=false --ac6_force_loadout_launch=false \
  --user_data_root=/fastdata/lavaulta/auto-re-agent/reports/logs/cycle-524-loadout-native-ready-control/user-data
```

The harness was intentionally interrupted after the final capture; exit 130 is
not a runtime crash.

## Artifacts

- Binary SHA-256:
  `1910466ee2f7d7572fa39e4c8464bf12701d581ea7112de975721cf55e187460`.
- Current log SHA-256:
  `06ddb3c682722e0d618b8cf923b1aeb2277a29f1037ea4ac96f78fa69418bdad`.
- Rotated evidence log SHA-256:
  `b3fa69239f482b8e03ecea6f4caf4d3276e1396511ae612f50fc5805085a5bf9`.
- Aircraft capture SHA-256:
  `60cf4314d05303c5ca02757d6621f342e1c7791d51d4ef9fca7a5f9e0b4cc36c`.
- Native observation capture SHA-256:
  `7fcd43ca618ccfa0555ad96409309d283c3f2b561fca8b060d4b65f7bd1d4717`.

## Validation and next boundary

Build and test validation remain those recorded in cycle 523: AC6 6/6 pass;
full CTest 1,613/1,619 with the six separately recorded failures.

Next:

1. Find stores and callers for `level_root+0x276A0` bit `0x20` in the canonical
   PAL project.
2. Identify whether the bit is campaign-mode policy, resource completion or a
   missing service publication.
3. Repair only that producer/route and require the natural event sequence
   `207 -> manager+35994=1` with both force options disabled.
4. Then test whether capability statistics populate; keep that publication
   contract separate if readiness alone is insufficient.
