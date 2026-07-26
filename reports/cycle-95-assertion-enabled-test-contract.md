# AC6 native: assertion-enabled regression contract

Date: 2026-07-17

## Scope

This pass is limited to the native AC6 test contract.  It neither launches
Xenia nor requests a VNC, controller, keyboard, or other human action.

## Finding

The normal `RelWithDebInfo` configuration supplied `-DNDEBUG` to every CTest
executable.  The tests use C/C++ `assert`, so their assertions had been
compiled out.  A green result from that configuration therefore did not prove
the assertions ran.

`reconstruction/ace-combat-6/CMakeLists.txt` now applies `-UNDEBUG` only to
targets whose name ends in `-tests`, when tests are enabled and the compiler is
not MSVC.  Production libraries and installed tools retain the release
configuration.

## Corrections exposed by active assertions

* `unit_factory_tests`: a fixture intentionally lacking the direct callback is
  rejected, so `direct_context_rejected` is one, not zero.
* `link_821d1be8`: the fallback retail link walk now has its own bounded
  counter.  The direct scan count is retained for reporting and cannot suppress
  a valid fallback walk merely because it reached the same bound.
* `frame_direct_8222ccd0_tests`: the fixture that expects the `0x200` mapping
  now supplies bit `0x200` at retail offset `r28 + 0xe44`.

## Validation

Normal native build, after assertions were explicitly enabled for test targets:

```bash
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6/native
cmake --build .build/ace-combat-6/native -j16
ctest --test-dir .build/ace-combat-6/native -j8 --timeout 60 --output-on-failure
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
```

Result: **41/41** CTest tests passed; installation produced root-level
`bin/ac6-current-level-catalog` and `bin/ac6-scene-shell` without `bin/bin`.

The Clang ASan/UBSan configuration passes all non-scene-shell checks **24/24**
with assertions active.  `ac6-scene-shell-smoke` passes under sanitizer in
**0.12 s**, and the exact campaign smoke command also exits successfully under
an 8 s process guard.  A full CTest sanitizer run was not recorded as complete:
after previous interrupted parallel launches, CTest could leave the campaign
child computing despite the identical direct command succeeding.  That is a
harness/process-cleanup issue to reproduce separately, not evidence of a
human-required game action and not a reason to weaken the normal regression
gate.

## Boundary

These tests establish only the covered native contracts.  They do not establish
Xenia parity, a rendered retail frame, mission progression, or controller/UI
behaviour.
