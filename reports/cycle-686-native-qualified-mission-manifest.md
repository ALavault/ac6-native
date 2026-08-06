# Cycle 686 — qualified mission manifest gate

Date: 2026-08-03 (Europe/Paris)

The generic campaign state now accepts mission definitions through a physical
route manifest instead of trusting a mission number or selector alone.

## Contract

`CampaignMissionManifestEntry` records the logical mission spec plus the
expected DPL id/variant, DATA.TBL entry index, PAC bank, offset, stored size and
expanded size. `build_campaign_progression_from_manifest()` resolves the
selector through the existing mode-1 DPL and DATA.TBL contracts, compares every
recorded physical field, then delegates to the same generic progression builder.
It fails closed with `manifest_route_mismatch` or `manifest_extent_mismatch`
before any loadout or objective state is created.

## Deterministic test

The two synthetic missions use the same manifest gate:

```text
selector 1 -> DPL 9  -> DATA.TBL 9  -> 2 objectives
selector 2 -> DPL 10 -> DATA.TBL 10 -> prerequisite mission 1
```

The test still completes Mission 1, unlocks and completes Mission 2 through the
same state machine, and now additionally rejects a wrong physical index and a
wrong expanded size. The separate bounded loader test supplies the selected
PAC span and exercises stored/decrypted and malformed-resource paths.

## Validation and provenance

```text
CTest: 47/47 passed (41.00 s with -j2)
Targeted campaign progression + loader tests: 2/2 passed
git diff --check: clean

campaign_progression.h sha256:
  6b8bc147ab28a3bc10fe8f14ea5a39beb2074a312cb0f41ef2b437849fb28647
campaign_progression.cpp sha256:
  371d002945549e0b9b3333a961a11bd2dda86c9577837d7584c9322fa1baa1e9
campaign_progression_tests.cpp sha256:
  deaae27b9f2089312265dc196f3676287bb245dd8b285bf9052dd871594982c5
progression test executable sha256:
  4d7adcd6a0c7ac72a9b0888e7e0eab051fa2548d0669280953341036a75fe575
resource-loader test executable sha256:
  235301db06e6a3d4fafbb02e1d7a58f140b057414f520d1877b6134035901b94
```

## Boundary

The manifest is a native validation contract; its selector-2 values remain
synthetic and are not a claim about the retail Mission 2 payload. A future
retail manifest must carry the external DATA.TBL/PAC/XEX hashes and qualified
bounded slices before it is admitted. The next implementation seam is to feed
this manifest and loader into the AC6-owned Vulkan scene/gameplay shell, while
the empty bridge save profile remains a reason to avoid another oracle run.
