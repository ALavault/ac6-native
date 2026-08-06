# Cycle 692 — headless AC6-owned Vulkan backend

Date: 2026-08-03 (Europe/Paris)

The opaque Vulkan lifetime seam now has a real optional backend on hosts with
the Vulkan SDK/ICD.

## Backend path

`VulkanCampaignBackend::create()` creates:

```text
VkInstance
  -> graphics/compute queue family
  -> VkDevice + VkQueue
  -> VkCommandPool
```

`acquire(frame)` allocates a transfer-destination `VkBuffer` and bound device
memory sized from the decoded campaign resource. `submit_zero_fill(handle)`
records `vkCmdFillBuffer`, submits it to the queue and waits for completion.
The opaque handle is then released through
`CampaignVulkanResourceLifetime`, which destroys the buffer and frees memory.
No window/swapchain is required, so this boundary is suitable for headless CI.

## Deterministic test

`tests/vulkan_backend_tests.cpp` creates the backend, acquires a 4096-byte
campaign frame through the lease callbacks, verifies the resource exists,
executes the fill/submit/wait path, completes the generation lease and verifies
destruction. If no Vulkan physical device is available, the test reports a
bounded skip rather than changing native behavior.

## Validation and provenance

```text
CTest: 51/51 passed (40.71 s with -j2)
Vulkan backend smoke: passed (0.42 s on the local ICD)
git diff --check: clean

vulkan_backend.h sha256:
  252abba10963922d3ed10cf942c7c4335bec0ccd2c51852c2725d7edc69f0ef7
vulkan_backend.cpp sha256:
  dee43763b95f694ba86a9c192ec0f32ec610212bd5c78fd8b224fbd4b8c0b437
vulkan_backend_tests.cpp sha256:
  2c5d0638d00228f08a402a9e492ce6fe039094889e98c77c402168e6cb6f3ab5
backend test executable sha256:
  326f6ca37add766281eed8efaed05296877cc4a9343fa3698b7e417c42cd86dd
```

## Boundary

This is a headless resource/queue backend, not AC6 presentation. It currently
zero-fills a buffer because MATE/NTXR upload, descriptor binding, render pass,
swapchain and mesh submission are separate qualified seams. The retail Mission
2 route and non-empty save remain unresolved.
