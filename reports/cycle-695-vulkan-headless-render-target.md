# Cycle 695 — headless Vulkan render target

Date: 2026-08-03 (Europe/Paris)

The AC6-owned backend now creates and clears a real headless color target.

## Contract

`create_render_target(width, height)` allocates an optimal
`VK_FORMAT_R8G8B8A8_UNORM` image, color view, one-attachment render pass and
framebuffer. `clear_render_target()` records an inline render pass with a
caller-provided clear color, submits it and waits for queue completion.
Release destroys framebuffer, render pass, view, image and memory in dependency
order. No window or swapchain is required.

## Deterministic test

`tests/vulkan_backend_tests.cpp` creates an 8×8 target, verifies its opaque
handle, clears it with a deterministic RGBA color, then releases and verifies
that the target is gone. The same smoke still covers NTXR upload and descriptor
binding.

## Validation and provenance

```text
CTest: 51/51 passed (40.11 s with -j2)
Vulkan backend smoke: passed (0.43 s on the local ICD)
git diff --check: clean

vulkan_backend.h sha256:
  a9761d490935507643cc0d0eeb0571d9edfe97402805a50e74c4aaea35ff1f6f
vulkan_backend.cpp sha256:
  89ee1767a8535a46e040bfb26656d34fbea384aba7265b778f2605c48e7d430e
vulkan_backend_tests.cpp sha256:
  f8f34a7f0ae12cfe835f4d38710364a29bee4e6badb2e052f38854e7746a8493
backend test executable sha256:
  2d5e5f8b99ca77302632c0892cc59b27b914471c0da0a128063765f36fba79e6
```

## Boundary

This is a clear-only render target, not an AC6 frame. A qualified shader
pipeline, mesh/vertex upload, descriptor-to-draw path, readback/presentation
and swapchain remain open. The retail Mission 2 route and empty-save oracle
boundary remain unchanged.
