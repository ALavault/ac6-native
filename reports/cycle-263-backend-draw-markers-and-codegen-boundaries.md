# Cycle 263 — backend draw markers and codegen function boundaries

## Question

Can the guest MATE capture be followed by three distinct host-side markers,
and can the generated AC6 corpus compile without editing generated output?

Target identity: AC6 Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
image base `0x82000000`.

## Result

The Vulkan and D3D12 command processors now report three different events:

- entry into backend `IssueDraw` (`host_issue_called`);
- the boolean result returned by the backend (`backend_success`);
- recording of `Draw` or `DrawIndexed` in the host command buffer
  (`host_draw_emitted`).

The counters are split by backend and exposed both cumulatively and as
reset-safe per-frame deltas. `host_draw_emitted` means that a command was
recorded. It does **not** prove GPU completion or presentation.

The source verifier checks 24 structural invariants, including exactly one
entry marker, all successful returns passing through the result marker, and
exactly two emission markers immediately after the backend draw calls. The
stdlib-only telemetry unit test covers success, failure, non-emission, backend
separation, deltas and reset.

## Generated boundary correction

The first complete generated build exposed eight invalid C++ gotos to four
labels. They were caused by eight heuristic entries in `[functions]` that
started functions inside existing PPC loops:

`0x82393E30`, `0x82393EB8`, `0x8239D8A0`, `0x8239E970`, `0x823A0238`,
`0x823A0240`, `0x823A0298` and `0x823A02D0`.

None was a direct call target in the generated corpus. Removing only these
false starts from `ac6recomp_config.toml` and regenerating leaves one local
definition and one local reference for each affected label. All 50 generated
translation units, representing 23,368 functions, then compile. No generated
file was manually edited.

The Vulkan-only build also no longer compiles the D3D12-only texture override
source. This removes the false `d3d12.h` blocker while preserving that source
when `REXGLUE_USE_D3D12` is enabled.

## Build-profile separation

The codegen-only profile now builds `ac6recomp` as an object-library compile
target. It validates all native and generated translation units without trying
to link the deliberately absent UI/runtime layer. Normal builds still create
the real executable and retain their normal entry point and UI/runtime source
set; the codegen stubs were not expanded into a second runtime.

A normal Linux Vulkan runtime configure was not claimed in this cycle because
the host lacks the development packages `libgtk-3-dev` and
`libx11-xcb-dev`. `libvulkan-dev` and `libsdl3-dev` are installed. Installing
system packages is outside this repository-only change, so runtime link and
retail execution remain open boundaries.

## Validation

```text
backend marker source assertions: 24/24 PASS
backend telemetry unit: PASS
AC6 native CTest: 44/44 PASS (33.33 s)
generated AC6 translation units: 50/50 COMPILED
generated missing-label errors: 8 before, 0 after
codegen-only ac6recomp object target: PASS
git diff --check: PASS
```

Evidence: `artifacts/ac6-cycle263-backend-draw-marker-validation.log`.

## Archive qualification

No newly named or newly hashed AC6 archive was visible during this cycle. The
latest visible archive remained `ac6_material_bind_xex_boundary_v1.zip`,
SHA-256 `7acf3070750c9e7ac0aebf9fc37d1c3112adf859c946f3ac23d1346aaee5d0f6`,
already qualified and byte-identical to the earlier copy. A later upload must
be qualified by filename and hash before its conclusions are used.

## Limits and next autonomous step

- The three markers are statically implemented and tested but not yet observed
  in a retail run.
- No GPU completion or presentation is proved.
- No human, Xenia, GUI or VNC action is required for the next step.
- The headless compile target and real runtime target are now separated.
- A linkable Vulkan runtime still requires the normal GTK/X11-XCB development
  inputs and must be configured/tested separately; no retail result is claimed.
