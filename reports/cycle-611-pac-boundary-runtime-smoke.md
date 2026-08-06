# Cycle 611 — PAC boundary validation and runtime smoke

Date: 2026-08-03  
Target: AC6 PAL Xbox 360, `default.xex` SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`  
Canonical project: `ghidra-projects/ace-combat-6`  
Native worktree: `.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill`

## Result

The PAC hardening from cycle 610 is in the built executable. Completed guest
reads are copied into bounded owned buffers before archive reconstruction;
entry-size and range arithmetic are checked with 64-bit endpoints, and the
archive key is structured rather than bit-packed.

## Validation

- `cmake --build build-rt -j8 --target ac6recomp`: pass.
- `ctest --test-dir build-rt -R '^ac6_' --output-on-failure`: 7/7 pass.
- Runtime smoke: `reports/logs/cycle-611-pac-boundary-smoke/`.
  The 25-second bounded run captured at 0 s and 15 s, published 1,642
  `PRESENT` lines, and emitted no fatal/assert/unresolved/abort/segmentation/
  exception marker.
- Instrumented binary SHA-256:
  `b21605cdb4008be6bf62577f069ed21851b1eb5ac385e460abd4bb6c29f3c077`.

This closes the PAC evidence-integrity and startup-safety checkpoint. It does
not advance the Mission 01 frontier: the natural standby-to-HUD/readiness
producer and the capability-polygon data publication remain unqualified, and
both loadout force options remain disabled for acceptance runs.
