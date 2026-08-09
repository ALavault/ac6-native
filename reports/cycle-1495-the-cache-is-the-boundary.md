# Cycle 1495 — the cache is the boundary

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`; it was not opened
  or modified in this cycle.
- Retail sources stayed in `game-files`; the generated cache stayed outside
  the repository and no retail payload was added to the package or reports.
- Pre-existing Ghidra changes and untracked parser scripts were left untouched.

## Sealed retail content boundary

`ac6-native import --source DATA_ROOT [--cache CACHE_ROOT]` now qualifies the
PAL XEX, table and both PAC files, validates all 926 big-endian `DATA.TBL`
records with bounded 64-bit arithmetic, and imports campaign entries 9–23.
The C++ path implements the retail mode-1 XOR followed by raw DEFLATE, including
the raw mode, without invoking an extraction script.

The cache consists of SHA-256-addressed payload blobs, a versioned binary index
and a small atomic `current` pointer. The index retains the qualified source
identity, table index, PAC, group, codec, source range, stored/expanded/payload
sizes and both stored and decoded hashes. Files are synced before rename and
`current` is published last. Store opening verifies the index identity and
digest, record invariants and every payload blob before becoming valid.

`RetailSession` has a product overload taking a valid `RetailContentStore` and
`CampaignLoadout`. Mission ids 1–15 map explicitly to entries 9–23; the session
retains the loadout, table entry and content-index digest. The direct-payload
overload remains available for bounded tests and carries no product provenance.
Only Mission 01 remains marked playable-supported.

## Fifteen-mission coverage

The real import passed with:

```text
records                 15
decoded bytes           443168032
content index SHA-256   3573d7db36e70cc1f106506ba3f25900b7c226c414b84c3a7f23c4f27079d1de
```

The independent Python auditor reparses the binary index, verifies every blob,
cross-checks all ranges, sizes, codecs and hashes against
`ac6-pal-campaign-catalog.json`, and joins the recursive dependency inventory.
Two consecutive generations of `ac6-pal-campaign-import-matrix.json` produced
the same SHA-256:
`5e766e152c483e82ec8919b29cb08ab950040b990fb32dabf8077e15fecb26d2`.
The matrix covers missions 1–15 and entries 9–23 exactly, records observed
formats and unresolved common boundaries, and contains no cache path.

## Negative controls

The native tests reject a wrong source hash, truncated PAC, excessive size,
duplicate requested entry, duplicate PAC range, incomplete cache, incompatible
`current`, corrupted or internally inconsistent index, invalid raw/deflate
metadata and invalid loadout. A two-entry import that publishes its first
content-addressed blob and then encounters an invalid compressed record leaves
no `current`; abandoned staging is likewise not observable as a generation.
The same index shape and consistency rules have independent Python controls.

## Validation

```text
Release build, AC6_BUILD_PLATFORM=ON             pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb    62/62
tools/tests                                      86/86
real PAL import and store reopen                 pass, 15/15
independent cache audit                          pass, 15/15
matrix deterministic regeneration               pass
mission01-final-gate-v3 --require JF             pass
mission01-playable-gate-v1 --require JF          pass
contract addresses                              pass, 321/321
contract derivations                            pass, 52/0
contract artefacts                               pass, 146/146 match HEAD
```

## Not established

- JV and JP are not passed by this cycle.
- The cache closes campaign payload import and structural inventory, not the
  MATE/NDXR/NTXR bindings, camera/placement derivations or a second playable
  mission.
- The interactive frontend still does not launch Mission 01 from this store;
  controller, progression guard, persistence/replay identity, localisation and
  sustained 720p30 qualification remain open.
- No human controller session is requested yet.

## Next

Route the product Mission 01 scene readers through the sealed store and close
the scenario-to-model-to-material-to-texture bindings needed by the JV frame,
without importing diagnostic TSV constants or replacement geometry.
