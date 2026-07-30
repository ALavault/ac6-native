# Cycle 318 — present hook and runtime frontier

Date: 2026-07-27

## Qualified inputs

- Target: Ace Combat 6, Xbox 360 retail `default.xex`
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Image base: `0x82000000`
- Guest architecture: Xenon PowerPC64, big-endian
- Host: Linux amd64
- Compiler: `/usr/bin/clang-21`, Clang `21.1.8`

## Change

`ac6PresentTimingHook` moved from `0x821F05F8` to `0x821F05BC`.
The new address is after the swap-skip flag load and before its comparison, so
the hook observes every presentation decision rather than only the path that
survives the skip branch. The reproducible change is:

`patches/ac6-present-timing-hook-before-skip-20260727.patch`

Confidence: `confirmed` for instruction placement; runtime frequency remains
`unknown` because boot still stops earlier.

## Runtime advance

The reference regeneration removed only dynamically reached pseudo-function
cuts. This cycle consumed, in order:

- `0x8239E818`
- `0x823526F8`
- `0x82352758`
- `0x823575A8`
- `0x8234B9A0`

Earlier measured cuts retained by the same local reference configuration:

- `0x82345250`, `0x82345260`, `0x823452A8`, `0x82345300`
- `0x82349050`, `0x823493B0`, `0x821F60C0`, `0x8239E728`

These removals are `dynamic`, not headless-qualified function-boundary
evidence, and must not be promoted as verified reconstruction boundaries.

Observed runtime progression:

1. `0x823575FC -> 0x823574E0`, owning function `0x82357130`;
2. `0x8234B9C0 -> 0x8234B9A0`, first in generated
   `rex_sub_8234B9A0`;
3. after removing that pseudo-entry, the same branch is owned by
   `sub_8234B978`, proving the removal changed the generated partition.

Current next measured split candidate: `0x8234B9A8`.
Current generated unresolved-branch trap count: `4815`.

## Validation

- Code generation completed successfully.
- `cmake --build ... --target ac6recomp -j16`: passed with Clang `21.1.8`.
- Native executable SHA-256:
  `8a6186c7dc0cae29a2c52f8ec9237c96ed7772b25bdf59b9b6194bead71811dd`.
- 20-second Xvfb/GDB smoke: deterministic `SIGABRT` at generated line 25878,
  unresolved branch `0x8234B9C0 -> 0x8234B9A0`.
- CTest: command passed, but this build declares no tests.

## Open boundary

The executable is neither playable nor parity-qualified. Continue the
measured partition repair at `0x8234B9A8`, then re-run codegen, the Clang 21
build, and the runtime smoke before attempting presentation-frequency
measurement.
