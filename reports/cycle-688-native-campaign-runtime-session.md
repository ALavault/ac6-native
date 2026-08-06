# Cycle 688 — transactional native campaign runtime session

Date: 2026-08-03 (Europe/Paris)

The native campaign pipeline now has a renderer-neutral gameplay session that
joins manifest qualification, bounded PAC loading and progression events.

## Contract

`CampaignRuntimeState` owns only `CampaignProgressionState` and an optional
`CampaignResourcePayload`; it has no SDL, Vulkan or guest-pointer dependency.

```text
qualified manifest
  -> progression state
  -> mission selection + bounded resource load (transaction)
  -> loadout
  -> briefing / active objectives
  -> completion + prerequisite unlock
  -> resource release
```

`select_campaign_runtime_mission()` snapshots the progression state before
selection. If the PAC bank is missing, truncated or undecodable, it restores
that snapshot and clears the active payload. Start, objective and completion
events require the selected resource and delegate to the same generic
progression functions. Completion releases the payload after recording the
state transition.

## Deterministic test

`tests/campaign_runtime_tests.cpp` builds two manifest-qualified synthetic
entries (encrypted stored payloads at DATA.TBL indices 9 and 10), completes
Mission 1, unlocks and completes Mission 2 through the same runtime session,
and verifies the decoded payload identity for both. A selection against an
empty PAC span is rejected transactionally and leaves Mission 1 `available`.

## Validation and provenance

```text
CTest: 48/48 passed (40.22 s with -j2)
Targeted campaign progression/loader/runtime tests: 3/3 passed
git diff --check: clean

campaign_runtime.h sha256:
  ce430828652d0aa40acf8baa4ab7e1d55af3d4939a12859772ba6a4f4477396a
campaign_runtime.cpp sha256:
  06a75430c97b175235eac20409b001df9b6e7f0023cf773bfe86d23e4ed371f1
campaign_runtime_tests.cpp sha256:
  faa9017087878b88dbc63eb56295b77789100aa8351ce905d02d0d4279faefe1
runtime test executable sha256:
  56578b9ddcc267f504502d1007f1b751a2d070c7998335c5a70f63d85ce8f359
```

## Boundary

The test uses synthetic manifest entries and payloads; it is not a retail
Mission 2 result. The next seam is a Vulkan-owned frontend that consumes this
session's resource and progression events, plus an externally hashed retail
manifest/slice set. The available bridge save is empty, so no new oracle run is
justified until a non-empty save or controlled state window is supplied.
