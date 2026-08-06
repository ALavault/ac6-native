# Cycle 612 — PAC index regression and runtime smoke

Date: 2026-08-03  
Target: AC6 PAL Xbox 360, `default.xex` SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`  
Canonical project: `ghidra-projects/ace-combat-6`  
Native worktree: `.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill`

## Result

The PAC hardening is now covered by a focused regression test. A synthetic
DATA.TBL with a DATA00 offset whose bit 31 is set and a DATA01 row remains
independently addressable; overlapping-range queries select the correct
archive, and a range crossing the 4 GiB addressable boundary is rejected
without replacing the last valid index.

The dumper also bounds LZX output, publishes blobs through a temporary file and
atomic rename, and refuses malformed or oversized entry ranges.

## Validation

- `cmake --build build-rt -j8 --target ac6recomp ac6_pac_index_test`: pass.
- `ctest --test-dir build-rt -R '^ac6_' --output-on-failure`: 8/8 pass.
- Runtime smoke: `reports/logs/cycle-612-pac-index-runtime-smoke/`.
  The bounded 25-second run captured at 0 s and 15 s, published 1,655
  `PRESENT` lines, and emitted no fatal/assert/unresolved/abort/segmentation/
  exception marker.
- Instrumented binary SHA-256:
  `8987d3497f439aa67174f82f3774f4f16c6a841ea9ecd125733b1a0d4f6911ad`.

This strengthens the evidence pipeline but does not advance the Mission 01
frontier. The natural standby-to-HUD/readiness producer and the capability
polygon data publication remain unqualified; acceptance runs keep both
loadout force options disabled.
