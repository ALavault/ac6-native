# Final verification

- [x] C++ differential accepts an exact spatial source.
- [x] EDRAM mismatch is localized before later stages.
- [x] Copy mismatch reports x, y, channel and byte values.
- [x] Padding-only mismatch remains distinct from pixel mismatch.
- [x] Simultaneous pixel and padding mismatch preserves first-stage ordering.
- [x] Invalid normal, EDRAM and tiled extents fail closed.
- [x] `require_exact_reached_copy` rejects every non-exact certificate.
- [x] Optimized C++20 test passes.
- [x] AddressSanitizer test passes.
- [x] UndefinedBehaviorSanitizer test passes.
- [x] Python spatial self-test passes.
- [x] Python bytecode compilation passes.
- [x] Shell syntax validation passes.
- [x] No GitHub Actions or pull request was used.
- [x] No ZIP, proprietary payload or runtime image was published.
- [ ] Product Vulkan EDRAM/tiled bytes have not yet been supplied to the certificate.
- [ ] Non-black guest writeback remains disabled.
