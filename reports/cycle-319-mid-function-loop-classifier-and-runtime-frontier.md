# Cycle 319 — mid-function loop classification and runtime frontier

## Target

- Product: AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module: `default.xex`
- Module SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Image base: `0x82000000`
- Route: `deterministic-fast-path`

## Result

The code generator now emits conditional branches to addresses contained in an
actual block of the current function as internal labels. This removes the
incorrect cross-function call previously produced for the backward loop
`0x8233C3C8 -> 0x8233C3B8`.

The configured fragment entries `0x8237FD20` and `0x82122398` were redundant
with their complete parent functions and were removed. Regeneration now emits:

```cpp
if (ctx.cr0.eq) goto loc_8237FD20;
...
if (ctx.cr6.gt) goto loc_8237FCE8;
```

The former fatal runtime frontier
`0x8237FD20 -> 0x8237FCE8` is therefore cleared without editing generated
output. The same correction clears the subsequently observed fatal branch
`0x821223E4 -> 0x8212233C`: both targets now remain internal labels in the
complete generated function.

## Validation

- Codegen: success, 23,300 functions generated after removing the second
  redundant fragment.
- Full build: success with `/usr/bin/clang-21`, Clang 21.1.8.
- Native executable SHA-256:
  `2f95bf266848a198c964da2ded7da13367e34bf021c4ee754e553904f9530f3c`.
- CTest: command succeeded, but this build configuration registers no tests.
- Bounded Xvfb runtime: 45 seconds, terminated only by timeout (`124`), with no
  fatal branch or process abort.

## New frontier

The GDB stop at Xbox 360 PAL address `0x821E2B78`, with guest destination
`0xB9C601DC`, was not an application crash. This address belongs to the
SDK-backed physical aperture `0xA0000000-0xBFFFFFFF`; the application signal
handler recovers stale page protection and resumes execution. GDB had stopped
before that handler. Confidence: `confirmed` from the SDK mapping and the
outside-GDB execution.

The first real process termination outside GDB was the fatal branch
`0x821223E4 -> 0x8212233C`. Removing only the redundant configured fragment
`0x82122398` restores the complete parent loop and clears it. No later fatal
boundary appeared during the 45-second headless run. The next bounded frontier
is therefore runtime interaction/render acceptance rather than another
confirmed CPU control-flow abort. Route: `needs-dynamic-evidence`.

This is progress evidence, not playability or retail-parity evidence.
