# AC6 PAL demo renderer — stage-local EDRAM/copy differential

Date: 2026-08-20
Target: reached 640×360 RGBA8 normal draw and 1280×720 tiled copy destination

## Result

This pass turns the existing CPU copy oracle into a **stage-local differential
certificate**. Given the normal-draw readback and the observed tiled resolve,
and optionally the observed EDRAM audit buffer, it reports the first failing
stage rather than one undifferentiated output hash:

```text
normal RGBA8
→ EDRAM materialization
→ copy/convert pixels
→ destination tiled padding
```

The possible first-failure labels are:

```text
exact
edram_materialization
copy_pixels
destination_padding
```

The runtime path is not modified by this commit. No non-black frame is promoted,
and guest writeback remains subject to the existing qualification.

## 1. Differential contract

The C++ entry point is:

```cpp
ReachedCopyDifferential diagnose_reached_copy(
    std::span<const std::byte> normal_rgba8,
    std::span<const std::byte> observed_tiled,
    std::span<const std::byte> observed_edram = {});
```

It rebuilds the expected state with the already bounded profile:

```text
normal source              640 × 360 × 4 = 0x0E1000 bytes
EDRAM audit allocation     0xA00000 bytes
EDRAM reached surface      0x384000 bytes
copy output                1280 × 720 × 4 = 0x384000 bytes
tiled destination          0x398000 bytes
non-pixel padding           0x014000 bytes
copy byte map              [2,1,0,3]
```

The EDRAM input is optional because the product runtime may expose only the
normal readback and the compute output in an early audit run.

## 2. Evidence emitted

The certificate records:

- SHA-256 of the normal readback;
- expected and observed EDRAM SHA-256, when supplied;
- expected and observed linear copy SHA-256;
- expected and observed tiled SHA-256;
- number of mismatched EDRAM bytes;
- number of mismatched copy bytes and pixels;
- number of mismatched padding bytes;
- first EDRAM byte mismatch;
- first copy mismatch as `(x, y, channel, expected, observed)`;
- first padding mismatch as a tiled allocation offset.

`require_exact_reached_copy` converts any non-exact certificate into a
fail-closed `RuntimeTrap` naming the first failed stage.

## 3. Why pixel and padding differences are separated

The reached tiled allocation contains 3,686,400 pixel bytes and 81,920 bytes
that are not addressed by any pixel coordinate. A full-buffer mismatch alone
cannot distinguish:

```text
copy shader wrote a wrong pixel
from
copy shader crossed into padding
```

The differential constructs an address mask from all 921,600 reached pixel
coordinates. It untile-compares pixel bytes and separately checks every
non-pixel location against the audit canary `0xA5`.

A padding-only change therefore reports:

```text
linear_pixels_exact      true
destination_padding_exact false
first_failed_stage       destination_padding
```

## 4. Optional EDRAM stage

When a 0xA00000-byte EDRAM audit dump is supplied, the certificate rebuilds the
expected buffer from the normal readback:

```text
one source pixel
→ four 2×2 sample positions
→ 80×16-sample tiles
→ 16-tile pitch
→ 0x5A outside the reached surface
```

An EDRAM mismatch takes precedence over later copy or tiling mismatches because
it is the earliest available failed stage.

## 5. Independent command-line audit

```bash
python3 tools/diagnose_reached_copy.py \
  --normal-rgba normal-640x360.rgba \
  --tiled-resolve resolve-1280x720.tiled \
  --edram edram-0xa00000.bin \
  --json differential.json
```

The Python implementation independently rebuilds the EDRAM, copy conversion
and Xenos tiled destination. Exit status is zero only for an exact result.

The self-test uses a spatially varying source and records:

```text
normal SHA-256
6e02bf2814ceec2aef652db55f82e9134f24d666168ff9f0ac03c9c84934d52a

EDRAM SHA-256
6bd620b37a40e04e61a19a4cf0d44fd1c92be490d29d4951e4841fd076788758

linear copy SHA-256
60cd4712373e822090d1e9b5ba8b5625c741c72843100c11bb1c9cbe9c19bad5

tiled copy SHA-256
72b170ddef97e028ea41b6981c508d249aef09f098a0e619c02b37dd1498e3a4
```

These are self-test artifacts, not game-frame claims.

## 6. Validation

The C++ suite exercises:

- an exact spatial source;
- a one-byte EDRAM mutation;
- a one-channel copy-pixel mutation;
- a padding-only mutation;
- simultaneous pixel and padding mutations;
- first-failure ordering;
- invalid normal, EDRAM and tiled extents;
- fail-closed promotion through `require_exact_reached_copy`.

It passes in optimized, AddressSanitizer and UndefinedBehaviorSanitizer builds.
The Python self-test and bytecode compilation also pass.

## 7. Next runtime integration

The next bounded change is:

```text
execute_vulkan_neutral_resolve
→ retain the observed EDRAM audit bytes
→ retain the observed tiled compute bytes
→ call diagnose_reached_copy
→ require exact CPU/GPU equality
→ permit non-black guest writeback only after exact certification
```

Until that integration is made, this commit is an audit oracle and does not
alter default renderer behavior.

## Audit notes

- The CPU and Python implementations are independent code paths but share the
  published Xenos layout formulas.
- The oracle models the reached profile only; it is not a complete EDRAM model.
- A matching certificate proves consistency with the bounded oracle, not visual
  fidelity to physical Xbox 360 hardware.
- The EDRAM input is an audit buffer whose outside-surface canary is part of the
  contract, not a generic hardware dump.
- No GitHub Actions, pull request, ZIP, proprietary payload or runtime image is
  part of this change.
