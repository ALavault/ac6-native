# AC6 `0x8236bf20` dynamic track consumer

Date: 2026-07-15 (Europe/Paris)

## Evidence boundary

This pass starts from the first-world MOP join and searches for the nearest
runtime consumer of the same dynamic-transform concept. It closes a cinematic
track update path, not player flight input or a player-aircraft spawn.
Address-based function names are retained.

The pre-existing re-agent export contained 8,824 functions but disagreed with
the corrected PowerPC XEX project at `0x8236bf20`. A fresh re-agent/Ghidra
bridge cache was therefore generated from `ace-combat-6-corrected/default.xex`;
it contains 15,333 functions. Re-agent dry-runs were performed at
`0x8236bfa0` and `0x8236eab0`. No LLM call or speculative source generation was
accepted. The cache was removed after the validation gate; the exact retained
instruction evidence is
`reports/function-8236bf20-dynamic-track-consumer.log`.

## Exact executable observations

`0x8236bf20` preserves its two floating inputs and rejects an absent track list
at object `+0x18`, absent state at `+0x20/+0x24`, an integral first input outside
the inclusive `+0x28..+0x2c` interval, and nonpositive mode `+0x08`.

For modes 1 and 2 it walks the `+0x14`-count array at `+0x18` and calls
`0x8236eab0` for every entry. Mode 3 first compares the value returned by
`0x82356180(track)` with object `+0x0c`, updates the matching entry, then updates
the remaining entries. Each call passes object `+0x04` plus the two preserved
floating inputs.

The independently decompiled `0x8236eab0` tests track `+0x20`, calls
`0x8236e658` with the integral frame, indexes track `+0x10`, computes the
fractional distance from the selected key time and forwards the result through
`0x8236bac0`. The latter is a virtual dispatch through object `+0x98`, so the
concrete property interpolation remains open. This is sufficient to identify
an address-bounded dynamic track consumer, but not to name its class or claim a
specific interpolation formula for every track type.

## Native observable and interaction

The SDL shell already uses dictionary-proven `MoveCamera` frames to sample the
joined MOP tracks. `--capture-frame CUT_FRAME PATH.bmp` now exposes that same
observable deterministically; the interactive Left/Right, PageUp/PageDown,
Home and Space controls select or advance the same frame index.

For the first joined aircraft:

| CUT frame | position | orientation |
| ---: | --- | --- |
| 1 | `[-17376.5, 2.00292, -2047.88]` | `[0, -0.421749, 0]` |
| 120 | `[-17391.4, 2.00292, -2014.19]` | `[0, -0.413543, 0]` |

Visible evidence:

- comparison: `captures/first-cut-dynamic-transform-comparison.png`, SHA-256
  `0fb6c64289a561e8466d8110898d58cc1dfb6a42c0536af0a45b0daf54dbd978`;
- frame 1 BMP: SHA-256
  `39e49d9a6dc9bf295ab85cdb548fb1bcc56aec5e9d9c999fca9b7035292febda`;
- frame 120 BMP: SHA-256
  `b9b14f5d026da42d4202667943c9c2e90196e83fa4378fbfc047714b72f3d1e9`.

The camera marker, camera direction and both aircraft transforms visibly
change. This is an exact cinematic interaction and remains
`world_renderer=native-partial`; keyboard/controller flight dynamics and the
player camera are still open.

## Validation

- GCC build and CTest: 14/14 pass.
- Clang AddressSanitizer plus UndefinedBehaviorSanitizer: 14/14 pass.
- Sanitized frame-1 and frame-120 BMPs are byte-identical to the GCC outputs.
- Only `.build-ac6-linux` and `.build-ac6-clang-san` remain as active AC6 build
  caches after the re-agent export cache is removed.
