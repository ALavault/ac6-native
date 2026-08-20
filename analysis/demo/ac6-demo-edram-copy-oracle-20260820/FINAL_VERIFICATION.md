# Final verification

- [x] Source extent is fixed to 640×360 RGBA8.
- [x] Destination extent is fixed to 1280×720 RGBA8.
- [x] EDRAM sample coordinates are bounded to 1280×720.
- [x] Every source pixel is copied to four 2×2 sample locations.
- [x] EDRAM bytes after the reached 0x384000-byte surface preserve 0x5A.
- [x] The copy oracle performs byte mapping [2,1,0,3].
- [x] The asymmetric linear digest matches the pinned Vulkan harness.
- [x] The asymmetric tiled digest matches the pinned Vulkan harness.
- [x] A spatially varying pattern round-trips through destination tiling.
- [x] A one-byte destination mutation is detected.
- [x] Invalid extents fail before mutating caller buffers.
- [x] C++ test passes under ASan and UBSan.
- [x] Python audit self-test passes.
- [x] Python bytecode compilation passes.
- [x] No GitHub Actions or pull request is used.
- [x] No ZIP or proprietary game byte is published.
- [ ] Non-black runtime Vulkan output has not yet been compared to the oracle.
- [ ] Guest writeback remains behind the current runtime qualification.
