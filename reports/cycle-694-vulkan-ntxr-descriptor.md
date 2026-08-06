# Cycle 694 — NTXR image view and descriptor binding

Date: 2026-08-03 (Europe/Paris)

The AC6-owned Vulkan backend now binds the uploaded NTXR image to a real
combined-image-sampler descriptor.

## Contract

After `submit_texture_upload()` transitions the image to
`SHADER_READ_ONLY_OPTIMAL`, `bind_texture_descriptor()` creates (lazily) a
linear-repeat sampler, a one-binding fragment descriptor layout and a bounded
descriptor pool, then allocates and updates a set with the image view and
shader-read layout. Descriptor sets are freed before image/view destruction.

The portable `VulkanMaterialBinding` remains the source of qualified identity,
format/view and shader facts; this backend does not reintroduce D5B4-specific
hash branches.

## Deterministic test

`tests/vulkan_backend_tests.cpp` now asserts the full sequence:

```text
NTXR RGBA8 decode -> staging/image -> layout/copy/queue wait
  -> image view + sampler + descriptor set
  -> descriptor free + image/staging destruction
```

The headless test passes on the local ICD.

## Validation and provenance

```text
CTest: 51/51 passed (40.12 s with -j2)
Vulkan backend smoke: passed (0.42 s on the local ICD)
git diff --check: clean

vulkan_backend.h sha256:
  72791a4529b3c026595eb69f2e47286c27d32ac7d7f678c5b5142dd70edfcd57
vulkan_backend.cpp sha256:
  711dc75742f18e4d132a647c1b6fd2ce08e0a12def356e70078b87c046f8cc2b
vulkan_backend_tests.cpp sha256:
  d9db6cd7540504769b9a7cc58299a4f0f473151d99947ad3029e7fa1be8f9829
backend test executable sha256:
  75a54f37519aac323352289e0b9987d78f4a261e2b6abf83b69922ef29f78d2b
```

## Boundary

This proves resource/view/descriptor setup, not a visible frame. A render
target/render pass, pipeline/shader module, mesh upload and swapchain remain
open. Retail Mission 2 routing and the empty-save oracle boundary are unchanged.
