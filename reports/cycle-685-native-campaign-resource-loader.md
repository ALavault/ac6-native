# Cycle 685 — bounded native campaign resource loader

Date: 2026-08-03 (Europe/Paris)

This checkpoint attaches a bounded PAC read/decode contract to the generic
campaign progression route. It does not read or copy an entire retail PAC.

## Contract

`reconstruction/ace-combat-6/include/ac6/campaign_resource_loader.h` exposes
read-only `CampaignPacBank` spans and
`load_campaign_resource(CampaignResourceRoute, pac_banks)`. The implementation:

```text
CampaignResourceRoute
  -> DATA.TBL bank_index()
  -> exact offset + stored_size range check
  -> copy only that stored range
  -> decode_payload(index, expanded_size, storage_class)
  -> stored + expanded bounded payload
```

DATA.TBL storage class `1` is passed through the existing raw-DEFLATE decoder
and class `2` through the encrypted stored path, matching the established
extractor contract. No filesystem path, guest pointer or archive-wide buffer is
introduced. A route selecting a missing bank, invalid catalog index, unknown
storage class, truncated range or undecodable payload fails closed with a named
error.

## Deterministic test

`tests/campaign_resource_loader_tests.cpp` uses a three-byte encrypted stored
payload at catalog index 39 and proves that the loader returns the exact stored
bytes plus the decoded `AC6` payload. It separately exercises missing-bank,
truncated-range, invalid-storage-class and malformed-compressed-payload paths.

## Validation and provenance

```text
CTest: 47/47 passed (40.21 s with -j2)
Targeted progression + loader tests: 2/2 passed
git diff --check: clean

campaign_resource_loader.h sha256:
  72694a07fea1093cc06fdbcc337bb37b82d99780986b427db1dce056e103a801
campaign_resource_loader.cpp sha256:
  2709042f6046ed573d7da01c47389bd57dbb4ebd45a0f22a8c03914bedceb05c
campaign_resource_loader_tests.cpp sha256:
  ae265737b5a514b6a84d8f47184531c92859f1cd164c05ab65f5188dfaedc387
CMakeLists.txt sha256:
  e7c0dedddfaa90b675f87430e9066f3d64c79dc15ff71debc90bca17165359e4
test executable sha256:
  235301db06e6a3d4fafbb02e1d7a58f140b057414f520d1877b6134035901b94
```

## Boundary

The loader is validated against synthetic bounded spans only. It does not claim
that a retail selector-2 route or its payload has been decoded. The next native
checkpoint is a hashed mission manifest that supplies qualified routes and
resource identities to this loader, followed by connecting the resulting
payload/resource events to the AC6-owned Vulkan scene/gameplay shell. The
available bridge profile remains empty, so no new oracle run is justified here.
