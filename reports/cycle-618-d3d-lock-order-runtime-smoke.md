# Cycle 618 — D3D lock-order correction and runtime smoke

Date: 2026-08-03

## Change

The frame-epoch correction now uses one lock order for the shadow state and
capture state: `g_shadow_mutex` (shared) precedes `g_capture_mutex` (unique) in
draw/clear/resolve capture and at the frame boundary. State setters already
followed that order before publishing their counters. Capture hooks use an
unlocked shadow snapshot only while holding the shared shadow lock, removing a
cross-thread inversion that could deadlock a setter against the UI/frame path.

## Validation

- Build: `cmake --build build-rt -j8 --target ac6recomp ac6_pac_index_test` passed.
- Focused AC6 CTest: 8/8 passed.
- Runtime: 25-second bounded smoke, 1,659 `PRESENT` lines, clean process
  teardown, no fatal/assert/unresolved/abort/segmentation/exception/deadlock
  marker.
- Binary SHA-256:
  `36ca874d6eecbcdab4dcd3fdbc60acd06187ecc913082d5e928d70b73cc223c9`.
- Log SHA-256:
  `a96cf265bcc6ddf2f33d2551d4a504033071097671bc8ba77a95f9f52a844427`.

Artifacts: `reports/logs/cycle-618-lock-order-runtime-smoke/`.

This validates the host telemetry correction only. It does not promote the
empty capability polygon or the Mission 01 standby screen to playable flight.
