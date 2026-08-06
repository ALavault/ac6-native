# Cycle 689 — native Vulkan campaign submission contract

Date: 2026-08-03 (Europe/Paris)

The transactional campaign runtime now has an explicit AC6-owned frontend
boundary for Vulkan submission. This is a backend-neutral contract; it does
not include Vulkan headers or pretend to issue GPU commands.

## Contract

`build_campaign_vulkan_frame()` accepts a `CampaignRuntimeState` and a
qualified `VulkanMaterialBindingResult`. It fails closed unless:

```text
active mission + active decoded resource
  + valid material binding
  + matching DPL/DATA.TBL route identity
  -> CampaignVulkanFrame
```

The resulting record gives the future Vulkan backend the mission status, DPL
resource id, DATA.TBL index, decoded payload byte count and the already
validated material binding. It performs no selector lookup, PAC read, guest
pointer translation or shader-hash workaround.

## Deterministic test

`tests/campaign_vulkan_frontend_tests.cpp` drives one manifest-qualified
runtime mission through resource load, loadout and active state, then verifies
the frame identity and D5B4 material contract fields. It separately rejects a
missing material and a payload whose route identity no longer matches the
mission definition.

## Validation and provenance

```text
CTest: 49/49 passed (40.74 s with -j2)
Targeted runtime + Vulkan frontend tests: 2/2 passed
git diff --check: clean

campaign_vulkan_frontend.h sha256:
  9ad437239ae8ea0e9669498e5935b6b473844b3024b553ea78b272a950db15eb
campaign_vulkan_frontend.cpp sha256:
  fd82bcbb3c76489a4a6fa6a897cf95c0f9ad8d9c0e621da4382c18ab4df838cb
campaign_vulkan_frontend_tests.cpp sha256:
  2d10d669146536d41f6658132cc4acf56356b46be1f2d13f43619107aa227ba4
frontend test executable sha256:
  91be7e5ce8178815aa23632d2202dfc646d32a28a387756b237030bbe95c58f7
```

## Boundary

This is a renderer submission contract, not a Vulkan implementation and not a
retail Mission 2 claim. The next implementation step is to map
`CampaignVulkanFrame` to the AC6-owned Vulkan command/resource lifetime while
retaining the material identity and view invariants. A non-empty retail save
and qualified selector-2 manifest remain prerequisites for runtime claims.
