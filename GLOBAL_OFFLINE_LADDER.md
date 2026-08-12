# AC6 PAL native Linux — global offline ladder

This is the single live roadmap for the handwritten native product. Mission 01
details remain in `MISSION01_LADDER.md`, but that file no longer defines product
scope or checkpoint order.

## Product boundary

The product is the PAL base game on Linux: persistent frontend, fifteen-mission
campaign, Free Mission, tutorial, aircraft and weapons, saves, replay, gallery,
English/Japanese voices, movies, and English/French/German/Italian/Spanish text.
Xbox Live, DLC, Marketplace, and Windows are outside this ladder.

Runtime code is handwritten C++. XenonRecomp/XenonAnalyse/XenosRecomp and Xenia
are bounded evidence tools and are never runtime dependencies. Retail data is
external and must be qualified by the PAL `default.xex` SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Public commands remain:

```text
ac6-native import --source DATA_ROOT [--cache CACHE_ROOT]
ac6-native play [--cache CACHE_ROOT] [--save SAVE_PATH]
ac6-native replay --cache CACHE_ROOT --replay FILE --report OUTPUT_DIR
```

`play` always enters the persistent frontend. `--frontend` remains a compatible
alias. Paths follow XDG.

## Checkpoints

| checkpoint | exit condition | state |
|---|---|---|
| 0 — baseline and global contract | baseline passes; historical contracts are fail-closed; mission contract template and 15-mission matrix exist | **passed** |
| 1 — complete offline import | atomic RetailContentStore v2 contains the qualified offline closure; no source PAC access after import | **passed** |
| 2 — Mission 01 structural blockers | VMX position, Scene/TCAM/MDLP/MATE/NDXR/NTXR, objective progression, and XMA/ASF close the executed M01 dependency cone; shared readers retain their 15-payload regression corpus | pending |
| 3 — complete Mission 01 vertical | M01 passes JF, JV, JP, and JG at 720p30 with exact 60 Hz simulation | pending |
| 4 — standard missions | M02–06, M08, M10–12, and M14 each pass JF→JV→JP→JG | pending |
| 5 — rich missions and campaign | M07, M09, M13, M15 and a deterministic complete campaign pass | pending |
| 6 — complete offline shell | frontend and all base-game offline modes, languages, voices, aircraft, weapons, and galleries pass | pending |
| 7 — hardening and distribution | performance, sanitizers/fuzzing, clean staging install, package and artefact audits pass | pending |

No mission is marked supported before all four of its gates pass. Shared-reader
changes rerun their corpus checks over all fifteen mission payloads.

## Mission gates

- **JF**: retail rules and final state are backed by qualified static evidence,
  native tests, and handwritten derivations.
- **JV**: the complete retail world is rendered by Vulkan with no synthetic
  manifest or interactive CPU fallback.
- **JP**: a human completes Normal/Normal with an Xbox SDL controller; the 60 Hz
  trace reproduces events, counters, result, and save exactly.
- **JG**: three stable Xenia oracle captures precede native comparison; global
  SSIM is at least 0.97, HUD/elements differ by at most 2 px, silhouette IoU is
  at least 0.98, cue onset differs by at most 20 ms, event timing by at most one
  tick, and level by at most 1 dB.

The machine-readable template is `analysis/templates/mission-gate-template.json`.
Current support state is `analysis/mission-capability-matrix.tsv`; both are
checked by `tools/audit_ac6_global_ladder.py`.

## Mission 01 execution spine

`analysis/mission01-execution-spine.json` is the live, ordered vertical slice:
retail load, controlled sortie, first objective, debrief, deterministic replay,
then parity. It limits feature work to the executed Mission 01 dependency cone
while retaining the fifteen-mission corpus requirement for shared readers.
The same global-ladder auditor checks its evidence hashes, prerequisite order,
three work lanes and oracle capture qualification. The mission matrix cannot
advance beyond this spine.

Checkpoint 2 follows the same M01-first scope. Semantic coverage of M02–M15 is
deferred until after the M01 preview; only shared-reader regression remains
global during the vertical slice. The durable decision is recorded in
`reports/cycle-1534-mission01-becomes-the-checkpoint2-semantic-scope.md`.

Normalized oracle and native event traces are compared with:

```sh
python3 tools/compare_ac6_execution_traces.py ORACLE.json NATIVE.json \
  --allow-legacy-diagnostic --report first-divergence.json
```

The report identifies the first sequence, tick and structured field that
differs; later mismatches are deliberately ignored until that first divergence
is closed. This command is currently diagnostic only. The comparator refuses a
parity gate until a runner-attested receipt has a verified implementation.

## Checkpoint closure

Each checkpoint requires a durable report, an isolated commit, JF, CTest,
Python tests, address/derivation/artefact audits, and the install check:

```sh
cmake --build reconstruction/ace-combat-6/build -j16
cmake --install reconstruction/ace-combat-6/build --prefix "$PWD"
test ! -e bin/bin
```
