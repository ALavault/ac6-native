# Cycle 693 — NTXR RGBA8 upload into Vulkan

Date: 2026-08-03 (Europe/Paris)

The headless Vulkan backend now consumes the portable NTXR RGBA8 decode and
uploads it into a real image resource.

## Backend path

`VulkanCampaignBackend::upload_texture()` creates a 2D
`VK_FORMAT_R8G8B8A8_UNORM` image with sampled/transfer-destination usage, a
device-local allocation and a host-visible/coherent staging buffer. The backend
copies the decoded pixels and records:

```text
UNDEFINED
  -> TRANSFER_DST_OPTIMAL
  -> vkCmdCopyBufferToImage
  -> SHADER_READ_ONLY_OPTIMAL
```

`submit_texture_upload()` submits and waits on the queue; `release_texture()`
destroys both image and staging allocations. This deliberately uses the
portable RGBA8 decode while the exact retail BC1/BC3 upload/view and descriptor
contract remains separate.

## Deterministic test

`tests/vulkan_backend_tests.cpp` now uploads a 4×4 decoded NTXR texture,
verifies the opaque texture exists, executes the transition/copy/submit path
and verifies destruction. The same test retains the campaign buffer lease
smoke.

## Validation and provenance

```text
CTest: 51/51 passed (40.45 s with -j2)
Vulkan backend smoke: passed (0.41 s on the local ICD)
git diff --check: clean

vulkan_backend.h sha256:
  7915dbb0dc1b2a0b3399849d2540231e11e6e2f721514a5e934d7be9e4fb3a38
vulkan_backend.cpp sha256:
  42e1bf32782f7f7a5d6ab53d4ca01e42720caf523f1631ebed40a5a895548ad7
vulkan_backend_tests.cpp sha256:
  995447e3779892997a25c7ab283ec05885bbf6d0b19284f503ddc6a8d971ecfb
backend test executable sha256:
  d712e877b039e7692f3a59d747a944b12013b743700a886d85f731581bc26198
```

## Boundary

This proves image allocation, staging, layout transitions and queue completion,
not a visible AC6 frame. BC1/BC3 native views, descriptor sets, render target,
mesh upload, swapchain and presentation are still unimplemented. Mission 2
retail routing and the empty-save runtime boundary remain unchanged.
