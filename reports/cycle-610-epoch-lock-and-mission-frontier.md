# Cycle 610 — frame-epoch publication and Mission 01 frontier

Date: 2026-08-03  
Target: AC6 PAL Xbox 360, `default.xex` SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`  
Canonical project: `ghidra-projects/ace-combat-6`  
Native worktree: `.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill`

## Runtime frontier

The corrected generator reaches the Mission 01 hangar without the former
`0x8236BC38 -> 0x8236BC0C` unresolved branch. Cycle 604 used both loadout force
options disabled and reached a rendered `MISSION 01 / STANDBY` screen, but every
capture after the input exercise was identical. This is a stable hangar
checkpoint, not a playable-flight result.

Cycle 607 qualified the natural campaign resource call at
`0x8218F3A0`:

```text
caller=0x8218F3A0 mode=1 selector=0xFFFFFFFF current_level=1 result=1
```

The new `ArmFirstMissionStage` path only publishes a bounded diagnostic epoch;
it does not write guest memory or force readiness/launch. Cycle 609 replayed the
cycle-539 profile with both force options disabled, but did not reach the
expected `type28=30` state before timeout. Its repeated pulse inputs are not
treated as loadout evidence.

## Correctness changes

`src/ac6_native_graphics.cpp` now protects `NativeGraphicsRuntimeStatus` with a
mutex. Runtime flags are synchronised into the protected object, frame results
are published after backend analysis, and the overlay receives a coherent copy.

`src/d3d_hooks.cpp` now uses `g_capture_mutex` as the frame epoch boundary:

- draw, clear and resolve counters are updated while their corresponding
  capture record is appended;
- non-capture counters use the same short critical section;
- `OnFrameBoundary` snapshots counters and swaps records while holding that
  lock;
- a draw can no longer be counted in frame N while its record is published in
  frame N+1.

`src/ac6_pac_index.cpp` now uses a structured archive/offset/size key and
64-bit interval endpoints. DATA00 offsets with bit 31 set can no longer collide
with DATA01 keys, and wrapped PAC entry ranges are classified correctly.

The PAC read dumper now copies each completed guest read immediately into a
bounded 64 MiB per-archive buffer. Entry reconstruction no longer follows a
guest pointer after the streaming buffer can be reused.

## Validation

- `cmake --build build-rt -j8 --target ac6recomp`: pass.
- `ctest --test-dir build-rt -R '^ac6_' --output-on-failure`: 7/7 pass.
- Full CTest run: 1,621 tests, six pre-existing failures (four NT
  epoch/calendar cases, the known `vpkd3d128_float16_4_invalid_0` case and the
  TemplateRegistry 11-vs-15 expectation) plus four intentional skips; no new
  AC6 failure.
- Runtime smoke: `reports/logs/cycle-610-runtime-status-epoch-smoke/`; 1,659
  `PRESENT` lines, captures at 0 s and 15 s, no fatal/assert/unresolved marker,
  bounded 25 s run completed normally.
- Instrumented binary SHA-256:
  `57e5f4c952945a1113afcb20398ad4877767d5132d49d375ab708bf10c90d71c`.
- `git diff --check`: pass in the native worktree.

## Next bounded question

Trace the native readiness publication and the capability-record producer from
the rendered standby hangar to the first HUD frame. Keep the acceptance profile
free of `ac6_force_loadout_ready`, `ac6_force_loadout_launch`, synthetic events
and guest writes. The capability polygon remains unqualified; do not attribute
it to the renderer until the loadout data publication is proven.
