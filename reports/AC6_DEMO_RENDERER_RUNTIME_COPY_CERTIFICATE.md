# AC6 PAL demo renderer — runtime copy differential certificate

Date: 2026-08-20  
Target: reached Vulkan neutral resolve and guest writeback path

## Result

The stage-local CPU differential is now enforced in the product renderer path.
The previously qualified Vulkan implementation is preserved byte-for-byte as:

```text
src/vulkan_neutral_resolve_original.cpp
Git blob e9bbe0b7c6c6a2fc145d77a24b7b98554e527e09
```

The public `execute_vulkan_neutral_resolve` wrapper performs:

```text
qualified Vulkan neutral resolve
→ returned 1280×720 tiled payload
→ independent CPU copy/tiling oracle
→ pixel and padding differential
→ exact certificate required
→ only then return to commit_reached_guest_present
```

This is an unconditional safety gate. `AC6_DEMO_WATCH_COPY_DIFFERENTIAL` only
controls the diagnostic line; it cannot bypass the certificate.

## Runtime trace

With:

```bash
AC6_DEMO_WATCH_COPY_DIFFERENTIAL=1
```

the renderer emits one bounded line beginning with:

```text
AC6_COPY_DIFFERENTIAL
```

The line includes:

```text
stage
exact
edram_provided
EDRAM, pixel and padding mismatch counts
normal, expected/observed linear and tiled SHA-256
first divergent pixel coordinate/channel
first divergent padding offset
```

The current wrapper does not receive the transient Vulkan EDRAM allocation, so
`edram_provided=0`. It distinguishes `copy_pixels` from
`destination_padding`; direct `edram_materialization` localization remains the
next integration step.

## Fail-closed behavior

The writeback is refused unless all of the following hold:

```text
normal readback extent = 640×360 RGBA8
observed tiled extent = 0x398000
all 921,600 output pixels equal the CPU oracle
all 0x14000 padding bytes retain the qualified 0xA5 value
tiled payload is byte-identical to the oracle
```

A mismatch throws a `RuntimeTrap` containing the full bounded trace line. The
existing guest-present join is therefore unreachable from a mismatched resolve.

## Validation

Targeted validation completed:

```text
optimized C++20 test                PASS
AddressSanitizer                    PASS
UndefinedBehaviorSanitizer          PASS
wrapper compile with Vulkan stubs   PASS
exact spatial frame                 PASS
single pixel-channel corruption     PASS
single padding-byte corruption      PASS
fail-closed exception text          PASS
```

The wrapper compilation smoke test macro-renames the historical implementation,
includes it in the same translation unit and verifies that the public function
returns only after the certificate succeeds.

No full Vulkan build, CMake build, CTest suite or PAL runtime execution is
claimed in this pass.

## Boundary

The current historical implementation still admits only the already qualified
black normal draw. This commit does not silently lift that gate.

The next bounded step is:

```text
retain the transient EDRAM audit allocation
→ pass it to certify_reached_copy_runtime
→ obtain direct EDRAM-stage localization
→ replace the black-only normal qualification with an explicit audit mode
→ allow non-black guest writeback only when EDRAM, copy pixels and padding are
  all exact
→ publish the resulting audit screencap
```

## Policy

- No GitHub Actions.
- No pull request.
- No ZIP or proprietary byte.
- No default renderer bypass.
- No gameplay screenshot claim.
