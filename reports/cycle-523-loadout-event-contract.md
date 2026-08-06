# Cycle 523 — loadout event contract

Date: 2026-08-02

## Qualification

- Target: Xbox 360 PAL `default.xex`.
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`.
- Native worktree HEAD: `6c417820fd6a89aade3cb02c53ddf4f222b81991`.
- Final runtime binary SHA-256:
  `1910466ee2f7d7572fa39e4c8464bf12701d581ea7112de975721cf55e187460`.
- Generated output was read for literal ABI/control flow only and was not
  modified.

## Outcome

The aircraft capability defect is upstream of rendering. The F-16C model and
hexagon axes render, but the capability polygon, named statistics and weapon
quantities remain empty because the native loadout lifecycle never publishes
readiness.

The previously suspected `manager+36120` word is not a persistent selected
aircraft id. It is a transient resource-loader candidate index:

1. It is initialized to `0xFFFFFFFF`.
2. Candidate 0 then candidate 1 reach counter 20 and are temporarily selected.
3. Their handles are consumed and resource pointers `0xA3780680` and
   `0xA3780700` appear.
4. With both candidate handles consumed, the transient index returns to
   `0xFFFFFFFF`.

The actual readiness byte is `manager+35994` (`0xB8E78F0A` for manager
`0xB8E70270`). A whole-scenario exact-address watch observed only zero writes;
no native write to one occurred.

Canonical handler `sub_8214D390` owns an indirect event table for ids 200–237,
and one table arm toggles readiness. Dynamic instrumentation proves that the
manager callback is registered, but the entire qualified loadout scenario
delivers only event `22`, twice. Event 22 is outside the handler table and
returns without changing state. The missing boundary is therefore the producer
or routing of the load-completion event, not an arbitrary readiness value.

No readiness field or event id was forced by this investigation. The existing
experimental `ac6_force_loadout_ready` remains necessary to cross the screen.

## Timeline contract requalification

Cycle 517 captured the first two repeat iterations at PAL caller `0x8237CEF0`:

- first `r6=0xB8EC9438`, second `r6=0`;
- repeat count starts at `0x5500554D`, then decrements to `0x5500554C`;
- `state+40=0xB8740180` points into UTF-16-like text, not a qualified repeat
  descriptor table.

This invalidates the old immediate-`r6`-producer hypothesis. The timeline fault
is an earlier table/owner initialization defect. Do not reuse `r6`, extend the
synthetic sparse child table, reinterpret child `+8` as an index, or retry data
address `0x82019E6C` as code.

## Dynamic evidence

Cycle 521 transitions:

```text
selected FFFFFFFF -> 0, candidates=2
  index0 primary=FFFFFFFF secondary=0000015B counter=20
  index1 primary=0000015A secondary=0000015B counter=20
selected 0 -> 1
  index0 c=A3780680 d=A3780700 primary=FFFFFFFF secondary=FFFFFFFF
  index1 primary=FFFFFFFF secondary=0000015B counter=20
selected 1 -> FFFFFFFF
  index0 c=A3780680 d=A3780700 primary=FFFFFFFF secondary=FFFFFFFF
  index1 c=A3780680 d=A3780700 primary=FFFFFFFF secondary=FFFFFFFF
```

Cycle 522 exact watch on `0xB8E78F0A`:

```text
sequence 1..3 lr=822CDB70 value=0
sequence 4..5 lr=821452BC value=0
sequence 6    lr=822CDB70 value=0
```

Cycle 523 manager events:

```text
event=22 state=0 b35988=0 b35989=1 b35990=0 ready=0 -> ready=0
event=22 state=0 b35988=0 b35989=1 b35990=0 ready=0 -> ready=0
```

## Reproducible scenario and diagnostics

Scenario:

- `scripts/ac6-first-mission-loadout-store-probe.steps`

The recipe uses qualified menu-state waits, stops before mission launch and no
longer relies on `PRESENT` as a state gate.

Store-watch hardening added to the instrumented build:

- `ac6_watch_store_skip_edges`: skip early matching edges so long windows are
  reserved for the loadout transition;
- `ac6_watch_store_address`: exact guest-address watch independent of capture
  timing;
- `ac6_watch_store_address_logs`: bounded exact-address log count.

Cycle 523 command:

```sh
tools/ac6-run.sh \
  --out /fastdata/lavaulta/auto-re-agent/reports/logs/cycle-523-loadout-event-contract \
  --duration 210 --display :133 --capture-at 0 --startup-timeout 120 \
  --keys '0:Escape:0.1,2:space:0.1' --wait-for 'type28=30' \
  --wait-pulse 'Escape+space:0.1:2' \
  --step-file /fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/scripts/ac6-first-mission-loadout-store-probe.steps \
  -- --ac6_log_ui_dispatch=true --ac6_log_loadout_dispatch=true \
  --ac6_force_loadout_ready=true --ac6_force_loadout_launch=false \
  --user_data_root=/fastdata/lavaulta/auto-re-agent/reports/logs/cycle-523-loadout-event-contract/user-data
```

The harness was intentionally interrupted after the final capture; exit 130 is
the harness shutdown, not a guest or host runtime fault.

## Artifacts

- `reports/logs/cycle-517-timeline-register-contract/`
- `reports/logs/cycle-519-loadout-long-store-contract/`
- `reports/logs/cycle-520-loadout-address-contract-corrected/`
- `reports/logs/cycle-521-loadout-selection-scan/`
- `reports/logs/cycle-522-loadout-ready-address-contract/`
- `reports/logs/cycle-523-loadout-event-contract/`
- Cycle 523 log SHA-256:
  `0a15dafcff45d4f9a5c5e5802a8c9a9af6a5fb16bbfb660b76dd43f70aec39fb`.
- Aircraft screenshot SHA-256:
  `b6a3aa3e2c6caf0c3d124f9c236d101e5cb45e6a42172c48cecccaff09318a60`.
- Final loadout screenshot SHA-256:
  `dba73fb53cc48f05d70d4b24d060e6d23c2cbc04cf562136f7def58b59f30d0b`.

The 32 MiB store captures in `build-store-519-runtime/` are isolated from and
do not overwrite the historical `build-store/` captures.

## Validation

- `cmake --build build-rt --target ac6recomp -j16`: pass.
- `ctest --test-dir build-rt -R '^ac6_' --output-on-failure`: 6/6 pass.
- Full suite: 1,613/1,619 pass; 6 fail and 4 skip.

The five previously known failures remain: four NT-epoch/calendar tests and
`ppc.test_vpkd3d128_float16_4_invalid_0`. A sixth, unrelated
`TemplateRegistry` expectation now reports 15 registered ids while the test
expects 11; a targeted rerun reproduces it. No loadout/probe change touches
that registry.

## Next boundary

1. Decode the 38-entry jump table at `0x8214D3B4` to name the exact readiness
   event id without guessing.
2. Trace that id backwards through the event bus to the resource-completion
   producer for the two loaded candidates.
3. Verify whether the producer is absent or its event is misrouted; repair only
   that contract and add a fixture for `resource loaded -> readiness=1`.
4. With both force options disabled, require populated capability data before
   returning to the separate timeline table-owner defect.
