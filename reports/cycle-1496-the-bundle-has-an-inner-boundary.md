# Cycle 1496 — the bundle has an inner boundary

## Correction to cycle 1495

Cycle 1495 correctly imported the complete 42,446,032-byte Mission 01 campaign
payload, but its store-backed `RetailSession` test put the already-extracted
scenario directly in the synthetic cache. The real cache stores a root FHM:
child 0 is the 3,477,248-byte scenario and child 1 is the 29,097,984-byte MDLP.
Passing the whole FHM to `ScenarioPayload` therefore did not test or implement
the claimed product boundary.

## Product boundary

`RetailCampaignBundle` now owns one qualified campaign payload and parses its
root table with the existing ports of `0x82234C18` and `0x82234DD0`. It checks
the `FHM ` identity, native endian/version, full four-array extent, child-count
limit and every non-empty child range with 64-bit arithmetic before exposing a
span. It retains mission id, DATA.TBL entry and content-index SHA-256.

The store-backed `RetailSession` opens this bundle, selects child 0 using the
declared array-1 length, copies only that bounded scenario span and then enters
the existing direct-scenario path. The test fixture now stores a real one-child
FHM rather than the scenario alone. An independently qualified fixture whose
child points outside the payload imports successfully at the content layer but
is rejected by both `RetailCampaignBundle` and `RetailSession`.

## Fifteen-payload control

The optional qualified-cache branch of `ac6-retail-session-tests` opens all 15
campaign bundles through the C++ reader. Every payload has 26 root slots, a
non-empty scenario at child 0 and an `MDLP` at child 1. It then opens Mission 01
through the same cache and verifies four sub-missions and 230 published units.
The qualified content-index identity remains:
`3573d7db36e70cc1f106506ba3f25900b7c226c414b84c3a7f23c4f27079d1de`.

## Validation

```text
Release build                                     pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb     62/62
tools/tests                                       86/86
synthetic valid/invalid FHM controls              pass
qualified PAL cache, bundle reader missions 1-15 pass
qualified PAL cache, Mission 01 RetailSession     pass, 230 units
mission01-final-gate-v3 --require JF              pass
mission01-playable-gate-v1 --require JF           pass
contract addresses                               pass, 321/321
contract derivations                             pass, 52/0
contract artefacts                               pass, 146/146 match HEAD
```

## Boundaries unchanged

This corrects cache-to-scenario routing only. JV, scene bindings, retail camera,
Vulkan persistence, JP progression, frontend/localisation and the human
controller session remain open. No Ghidra or oracle run occurred and no retail
bytes were committed.
