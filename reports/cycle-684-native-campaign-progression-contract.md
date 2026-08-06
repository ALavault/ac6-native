# Cycle 684 — generic native campaign progression contract

Date: 2026-08-03 (Europe/Paris)

This checkpoint moves the native reconstruction beyond the Mission-1-specific
scene-session guard without pretending that retail Mission 2 bytes are already
qualified.

## Contract

`reconstruction/ace-combat-6/include/ac6/campaign_progression.h` and its source
implement a renderer-neutral pipeline:

```text
CampaignMissionSpec
  -> mode-1 selector
  -> DPL resource id (0x821B6E58 contract)
  -> DATA.TBL index (0x821D1128 contract)
  -> CampaignResourceRoute
  -> available / loadout / briefing / active / completed
  -> objective completion
  -> prerequisite-based unlock
  -> deterministic save snapshot
```

The builder rejects empty entries, duplicate IDs, invalid objective counts,
missing prerequisites and prerequisite cycles. Loadout acceptance requires a
non-zero aircraft and weapon identity plus an explicit capability-data-valid
bit; no force-ready or mission-specific launch flag exists in this module.
The snapshot contains only completed mission IDs and bounded objective masks,
so it can later be attached to a native serializer without guest pointers or
renderer handles.

The previous `CampaignSceneSession` remains as a qualified selector-1/CUT
adapter. The new contract is the intended replacement boundary for future
mission definitions and does not claim that selector 2's physical payload has
been decoded.

## Deterministic test

`tests/campaign_progression_tests.cpp` builds two synthetic definitions through
the same route resolver:

```text
mission 1: selector 1 -> DPL 9 -> DATA.TBL 9, two objectives
mission 2: selector 2 -> DPL 10 -> DATA.TBL 10, prerequisite mission 1
```

It proves invalid loadout rejection, mission-1 completion, mission-2 unlock,
mission-2 launch through the same state machine, save snapshot ordering and
cycle rejection. No Mission 1 or Mission 2 branch appears in the implementation.

## Validation and provenance

```text
CTest: 46/46 passed (61.08 s)
Python asset tools: 8/8 passed
campaign progression test executable sha256:
  85ce5b4e58fa35d0c3547cc57c45d6f56eafcf53295b75e5a4c5f9cd3574953f

header sha256:
  adf930008e68965552f7b6c6a4b749f37208e5a48d0f9f336e29872d65838074
source sha256:
  5dab19b663525eb0eee217aa2b0043e71af761c66319caefb5eb499f14228539
test sha256:
  45698262896fe5284bf88ba6d9919257e49f478ff72bf9d7164d78137c823156
```

## Remaining boundary

The contract is native and generic, but its synthetic selector-2 route is not
retail asset evidence. The next implementation step is to feed it a qualified
mission manifest/resource loader and then connect its loadout/objective events
to the Vulkan-owned scene/gameplay shell. A populated retail save is still
required before claiming Mission 1 completion or Mission 2 unlock in the
bridge oracle.
