# Cycle 96 — AC6 Clang ASan/UBSan bounded revalidation

Date: 2026-07-17

## Scope

This is a non-interactive native validation pass. It does not launch Xenia,
VNC, a controller, a keyboard, or retail Xbox 360 content.

The existing Clang ASan/UBSan build has two materially different test classes:

- the first 24 CTest entries are bounded unit, archive, state and scene-data
  contracts;
- later scene-shell scenarios can be dominated by sanitizer runtime cost and
  are not silently made green by a larger timeout.

## Commands and results

```bash
cmake --build .build/ace-combat-6-flow-clang-sanitize -j16
ctest --test-dir .build/ace-combat-6-flow-clang-sanitize -I 1,24 --output-on-failure -j16
timeout --signal=TERM --kill-after=2s 8s env SDL_VIDEODRIVER=dummy .build/ace-combat-6-flow-clang-sanitize/ac6-scene-shell --smoke
git diff --check
```

The build was current. CTest passed **24/24** in **0.51 s**. The direct
`ac6-scene-shell --smoke` command completed within its explicit eight-second
process guard and left no `ac6-scene-shell` or CTest child behind. The working
tree diff check also passed.

## Boundary

This revalidates only the selected ASan/UBSan contracts and the one bounded
native smoke. It does **not** establish a full sanitizer scene-shell corpus,
Xenia behaviour, a retail rendered frame, or Xbox 360 parity. The broader
instrumented scene-shell slowdown remains `runtime-blocked` until it is
profiled with a separate, bounded experiment; this pass deliberately does not
increase its timeout.
