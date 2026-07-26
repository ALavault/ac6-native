# AC6 cycle 262 — MATE identity captured at the guest draw boundary

## Scope and identity

- target: `ac6-xbox360-pal`;
- module: `default.xex` / `ACE6_X360.exe`;
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- AC6Recomp base commit:
  `c5b089fb6988ac504ba394db611543bda2fb2c96`;
- capture boundary: `0x82364B44`, with `r30=request` and `r31=device`.

This pass implements the narrow capture proposed by cycle 261. It proves that
the generated guest translation calls the hook and that the identity latch is
consumed exactly once by the next compatible guest draw in unit tests. It does
not prove that a retail runtime execution reached the hook or that the host
backend emitted a draw.

## Reproducible headless code generation

ReXGlue now has an opt-in `REXGLUE_CODEGEN_ONLY` build. On non-Windows hosts it
replaces the native GTK window backend with narrow codegen stubs and keeps the
normal runtime configuration unchanged. The code generator builds with the
installed Clang 21 without installing GTK development packages.

The generator was run twice from the same inputs. The complete generated-file
SHA-256 manifest was byte-identical, and the generated hook call appears once,
at `0x82364B44`, immediately before the translated load of `request+0x08`.
Generated output was never edited manually.

## Capture contract

`ac6MateDrawRequestHook` performs bounded big-endian guest reads after checking
the request, material and device ranges:

```text
request
material = u32be(request + 0x24)
material_key = u32be(material + 0x00)
draw_context_key = u32be(request + 0x08)
device
```

The tuple is published to a single pending latch. `CaptureDrawCall` consumes it
only when the draw device matches, consumes it at most once, and includes the
five fields in the draw signature. A mismatch leaves the tuple pending for a
later compatible draw. Frame boundaries clear an unconsumed tuple so it cannot
leak into another frame.

The capture is enabled only with the existing `ac6_render_capture` switch. It
extends the existing draw sink rather than creating a second event stream.

## Validation

- ReXGlue codegen-only configure and `rexglue` build: **PASS**;
- two codegen runs: **PASS**, byte-identical generated manifest;
- generated hook occurrences: **1**;
- generated translation unit and `src/d3d_hooks.cpp`: **compile PASS**;
- latch/bridge standalone test: **PASS**;
- `VerifyMateDrawRequestKeyLifecycleContracts.java`: **50/50 PASS**;
- native AC6 CTest corpus: **44/44 PASS**;
- `git diff --check`: **PASS**.

The complete codegen-only runtime target still hits an unrelated portability
boundary: `src/ac6_texture_overrides.h` includes `d3d12.h` even in a Vulkan-only
configuration. This does not block the code generator, generated unit, hook
unit or native AC6 corpus and was not widened into this cycle.

Evidence:

- `artifacts/ac6-cycle262-codegen-hook-validation.log`;
- `artifacts/ac6-cycle261-request-queue-chain.log`;
- `artifacts/ac6-cycle261-mate-request-key-validation.log`.

## Remaining boundary

```text
MATE_IDENTITY_TO_NATIVE_GUEST_DRAW_CAPTURE_IMPLEMENTED
CODEGEN_REPRODUCIBLE_HEADLESS_CONFIRMED
RUNTIME_GUEST_DRAW_CAPTURE_NOT_OBSERVED
HOST_ISSUE_CALLED_NOT_OBSERVED
BACKEND_SUCCESS_NOT_OBSERVED
HOST_DRAW_EMITTED_NOT_OBSERVED
```

The next slice must add or qualify separate backend markers for
`host_issue_called`, `backend_success` and `host_draw_emitted`. Those markers
must not be inferred from the presence of the guest capture. No human action is
required for the current static boundary.
