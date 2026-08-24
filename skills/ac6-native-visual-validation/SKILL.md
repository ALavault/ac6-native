---
name: ac6-native-visual-validation
description: Validate AC6 native rendering with readback and human-visible captures.
---

# AC6 native visual validation

Use this skill for a rendering or presentation claim in the qualified PAL demo.
Logs such as `PRESENT`, a non-empty framebuffer, or a successful process exit
are not visual success.

- Run the qualified headless configuration with `SDL_AUDIODRIVER=dummy` and
  Xvfb when needed. Keep codegen ON for the reference run; test codegen OFF
  only when it is the explicitly named variable.
- Save the raw readback and a human-viewable screenshot in the gate artifact
  directory. Prefer `readback.ppm` plus `readback.png` (or the existing
  equivalent), and record dimensions, format, and capture timing.
- Validate pixels programmatically, then inspect the image. Check channel
  order, endian/swizzle interpretation, alpha, clear/background color,
  geometry coverage, and scaling independently. A colored image can still be
  a channel permutation or a partially rendered frame.
- When the readback and screenshot disagree, classify the boundary as
  readback, conversion/presentation, or capture tooling; do not silently use
  one as the other. A black, missing, or stale capture fails the visual gate.
- Preserve the actual image and the validator result under
  `artifacts/<gate>/`, with a short `RESULT.md` stating the expected visual
  witness, observed witness, and proof level. Human inspection remains
  required for logo/geometry recognition even when the pixel check passes.

Do not fix colors by guessing from a dominant channel or by forcing vertex
colors. Trace the qualified path from source data through vertex fetch,
shader/output format, Xenos copy/present state, and readback before changing
semantics. Keep demo PAL captures separate from retail captures.
