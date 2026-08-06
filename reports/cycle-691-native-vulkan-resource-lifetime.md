# Cycle 691 — native Vulkan resource lifetime seam

Date: 2026-08-03 (Europe/Paris)

The AC6-owned Vulkan boundary now has an explicit opaque resource lifetime. The
portable reconstruction still does not include Vulkan headers; a backend owns
the actual image/buffer objects through callbacks.

## Contract

`CampaignVulkanResourceLifetime` accepts a validated `CampaignVulkanFrame` and
an acquire/release callback pair. It enforces:

```text
valid frame
  -> one acquire callback
  -> generation + opaque handle lease
  -> reject second in-flight submit
  -> generation-checked completion
  -> one release callback
```

Missing callbacks, invalid frames, acquisition failure, duplicate submissions
and stale completions are rejected. A release exception leaves the lease in
flight so the backend can retry rather than silently losing ownership.

## Deterministic test

`tests/campaign_vulkan_lifetime_tests.cpp` uses callbacks that return and release
opaque handles. It proves generation 1/2 sequencing, single in-flight
enforcement, stale completion rejection, invalid-frame rejection and missing
callback handling.

## Validation and provenance

```text
CTest: 50/50 passed (40.70 s with -j2)
Targeted frontend + lifetime tests: 2/2 passed
git diff --check: clean

campaign_vulkan_lifetime.h sha256:
  8a20a8354df9d7ea2cf5fb9ffb16bc464c5abf0980292e4d7f90e10cb5d99653
campaign_vulkan_lifetime.cpp sha256:
  5e26d013aa0ed6b074a2ad9604c3f33d348c03ea0531ec24edc86d1a35ef6ad9
campaign_vulkan_lifetime_tests.cpp sha256:
  a410c44fff34243797cd4d1f8ca624323ed6c0a073022bf38a71b7f6c7f427a8
lifetime test executable sha256:
  14bebf1e7400e40e6449bc40ea548de081066d6b97cda5df90d50af4d3a6867f
```

## Boundary

The callbacks are deliberately opaque and therefore do not prove Vulkan
resource creation, queue submission, synchronization or presentation. The next
backend task is to implement those callbacks behind this contract and connect
them to the existing material/view invariants. Retail Mission 2 remains
unqualified and the empty bridge save still forbids a new oracle run.
