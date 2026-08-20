# Final verification

- [x] Historical Vulkan neutral resolve is preserved as its existing Git blob.
- [x] Public resolve is wrapped rather than duplicated or rewritten.
- [x] CPU differential is built from the actual normal readback and returned tiled bytes.
- [x] Exact certificate is required before the wrapper returns.
- [x] Trace enablement cannot bypass the certificate.
- [x] Pixel mismatches are localized by coordinate and channel.
- [x] Padding mismatches are localized by tiled offset.
- [x] Optimized C++ test passes.
- [x] ASan test passes.
- [x] UBSan test passes.
- [x] Wrapper smoke compile passes with a synthetic historical implementation.
- [x] No GitHub Actions or pull request is used.
- [x] No ZIP or proprietary payload is published.
- [ ] Transient Vulkan EDRAM bytes are not yet included in the runtime certificate.
- [ ] Non-black normal draw qualification remains closed.
- [ ] No PAL runtime screencap is claimed.
