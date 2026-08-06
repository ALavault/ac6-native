# Cycle 696 — Vulkan upload-readiness invariant

Date: 2026-08-03 (Europe/Paris)

The image descriptor path now records upload readiness explicitly. A texture
descriptor cannot be allocated while its image is still `UNDEFINED`, and a
second upload of an already transitioned image is rejected.

## Invariant

```text
upload_texture
  -> descriptor binding: reject
submit_texture_upload succeeds
  -> SHADER_READ_ONLY_OPTIMAL + descriptor binding: allow
second submit: reject
```

This keeps the Vulkan material contract fail-closed at the exact point where a
premature descriptor would otherwise produce undefined sampling.

## Validation and provenance

```text
CTest: 51/51 passed (40.51 s with -j2)
Vulkan backend smoke: passed (0.42 s on the local ICD)
git diff --check: clean

vulkan_backend.h sha256:
  a9761d490935507643cc0d0eeb0571d9edfe97402805a50e74c4aaea35ff1f6f
vulkan_backend.cpp sha256:
  9bba95a88dccf5da74a12286ea957165d74d0b59604f392418077876e101606f
vulkan_backend_tests.cpp sha256:
  0f72aaa47d651f19de47aa9bf9bd83a961a0bb316de955041561f2d2fa74535c
backend test executable sha256:
  aae824b450fdc45071d651dd7a7cb28ea61a285b0674db675ffd9e714a72dc6d
```

## Boundary

The backend still has no AC6 shader pipeline, mesh draw, readback or swapchain.
The next implementation must qualify those contracts before any visible native
gameplay claim; Mission 2 retail routing remains unresolved.
