# Cycle 687 — scene shell uses the bounded resource contract

Date: 2026-08-03 (Europe/Paris)

The SDL scene shell no longer duplicates the selector-to-DATA.TBL lookup,
selected PAC read and payload decode. It now supplies the generic loader with
two declared PAC sizes and file-range callbacks.

## Integration

`tools/scene_shell.cpp` now performs:

```text
parse + validate DATA.TBL
  -> resolve_campaign_resource_route(selector)
  -> CampaignPacBankSource[size, read(offset, length)]
  -> load_campaign_resource(route, sources)
  -> decoded Scene payload
```

The callback catches I/O failure as a named `read_failed` result. A short
callback result is rejected as `short_read`; the loader still performs the
physical range and storage-class checks before decoding. The shell therefore
uses the same route and decryption/decompression contract as future native
campaign code while retaining a read-only, range-bounded filesystem seam.

## Validation and provenance

```text
Targeted progression/loader/scene smoke tests: 4/4 passed
CTest: 47/47 passed (41.02 s with -j2)
git diff --check: clean

campaign_resource_loader.h sha256:
  14c01e7e9d6890fe5ec665a793be1c2b0f3490ec042fbc53c449c8263be72a06
campaign_resource_loader.cpp sha256:
  aea385b24d8a9e2f6d535bddd7ec70755a7c69d2775253a770a6c49a4f6fe58c
campaign_resource_loader_tests.cpp sha256:
  5645eca70eb92e1194b57c4eeeff31c211829010c3205a8a138842154bfe00fa
scene_shell.cpp sha256:
  645f59cf25bfa6f7a8f18a14aa620411c87f9d0b315a08d958da934986f3db48
scene-shell executable sha256:
  4921feff59cf07baa608a61d34c0c87fc39f05af9c634e3a605a4d5ccf70726c
resource-loader test executable sha256:
  3f8f98188945984ae37ae0fc8730c2bb420e1d8985d351ec0c0004ea747f4f9d
```

## Boundary

This proves native shell/resource plumbing, not retail Mission 2 content. The
next seam is to expose the generic progression state and qualified manifest to
the Vulkan-owned gameplay shell (loadout, objective events, completion and
save snapshot). A non-empty retail save and qualified selector-2 resource are
still required before claiming a Mission 2 runtime result; the current bridge
profile remains empty and no new oracle was run.
