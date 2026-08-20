# AC6 PAL demo renderer — non-black EDRAM/copy CPU oracle

Date: 2026-08-20  
Target: PAL Xbox 360 demo, reached 640×360 normal draw and 1280×720 copy/resolve

## Result

This pass closes a CPU-side oracle for the exact copy profile already reached by
the product renderer:

```text
640×360 RGBA8 normal-draw readback
→ four identical 2×2 EDRAM sample positions per source pixel
→ reached Xenos copy/convert profile
→ R/B destination swap
→ 1280×720 RGBA8 linear output
→ qualified Xenos tiled destination
```

The oracle is deliberately independent of the Vulkan resolve compute shader. It
can therefore distinguish:

```text
wrong normal-draw pixels
wrong EDRAM sample placement
wrong copy conversion or channel order
wrong destination tiling
```

instead of allowing all four faults to collapse into one black digest.

## 1. Evidence joined

The existing PAL copy profile fixes:

```text
RB_SURFACE_INFO       0x14000500
RB_COLOR0_INFO        0x00000000
RB_COPY_CONTROL       0x00100000
RB_COPY_DEST_BASE     0x1374A000
RB_COPY_DEST_PITCH    0x02D00500
RB_COPY_DEST_INFO     0x01000300
XE_SWAP               0x1374A000, 1280×720
```

The generic Xenos decode gives destination format 6 and `copy_dest_swap = 1`.
The pinned Vulkan harness independently observed that an asymmetric source word:

```text
11 22 33 44
```

becomes:

```text
33 22 11 44
```

Therefore the bounded byte conversion is R/B swap with G and A preserved.

The durable cross-check digests are:

```text
uniform asymmetric linear  66dde082635ccc6b24abba5b372ceb10173bc2b062faa2d93de7c4548bb60dc8
uniform asymmetric tiled   0bf69cf42fd6c3ac73b30c438a4db6d1664eaafa9c716b9ba330a9886c976786
```

## 2. EDRAM sample layout

The reached normal draw is 640×360 with four sample values. For the current
host readback, one resolved source pixel is replicated to a 2×2 sample block:

```text
sample_x = 2 * source_x + {0,1}
sample_y = 2 * source_y + {0,1}
```

The bounded EDRAM surface is represented as:

```text
sample dimensions     1280×720
sample bytes          4
sample tile           80×16
pitch                  16 tiles
surface bytes          0x384000
full EDRAM allocation  0xA00000
```

All bytes outside the reached `0x384000` surface retain the `0x5A` canary.
This keeps accidental over-reads observable.

## 3. New C++ contract

`include/ac6demo/reached_edram_copy_oracle.hpp` provides:

```cpp
reached_edram_sample_offset(x, y)
materialize_reached_normal_rgba8_edram(normal, edram)
build_reached_copy_linear_oracle(normal, linear)
build_reached_copy_tiled_oracle(normal, tiled)
reached_copy_tiled_matches_oracle(normal, observed)
```

The functions are fail-closed on every extent. Validation occurs before any
mutation, so malformed source or destination spans leave caller buffers intact.

## 4. Independent audit tool

`tools/audit_reached_copy_oracle.py` accepts:

```text
--normal-rgba    exact 640×360 RGBA8 readback
--tiled-resolve  exact 0x398000-byte resolve output
```

It rebuilds the expected linear and tiled output without using the product's
Vulkan shader, and reports:

```text
match
first mismatch byte
source SHA-256
expected and observed linear SHA-256
expected and observed tiled SHA-256
non-zero pixel count
```

`--self-test` reproduces both qualified asymmetric digests and confirms that a
single changed pixel is rejected.

## 5. Tests

The C++ test covers:

- known EDRAM sample offsets, including the final sample;
- rejection of out-of-range sample coordinates;
- four-sample replication;
- preservation of EDRAM bytes outside the reached surface;
- R/B swap on the first and final output pixels;
- qualified asymmetric linear and tiled SHA-256 values;
- a spatially varying 640×360 pattern;
- linear → tiled → linear equality;
- detection of a single changed destination byte;
- transactional rejection of an invalid source extent.

The test runs with strict warnings plus ASan and UBSan.

## 6. Runtime boundary remaining

This commit does **not** yet remove the product runtime's black-only guards.
The next integration is now mechanical and falsifiable:

```text
vulkan_neutral_resolve.cpp
→ validate normal readback digest against its own bytes, not a fixed black hash
→ materialize EDRAM with the new generic helper
→ build the CPU tiled oracle
→ run the Vulkan compute resolve
→ require exact CPU/GPU equality
→ only then permit guest writeback and an audit screencap
```

A non-black result must pass both the independent CPU oracle and the existing
transactional guest tiling path. Merely being colorful is not a credential.

## Verdict

| Frontier | Verdict |
|---|---|
| EDRAM sample addressing for reached profile | closed A- |
| Four-sample replication contract | closed A- |
| copy_dest_swap byte order | closed A- |
| 2× linear copy oracle | closed A- |
| tiled destination oracle | closed A- |
| CPU oracle versus uniform Vulkan harness | closed A |
| spatially varying Vulkan runtime comparison | open |
| non-black guest writeback | open |
| gameplay screencap | open |

## Audit notes

- The EDRAM mapping is a bounded host representation of this reached profile,
  not a complete Xenos EDRAM model.
- Replicating one resolved host pixel to four samples is correct for the current
  bridge contract; it does not recover per-sample values discarded by the host
  4×-MSAA resolve.
- The R/B swap is joined to the pinned generic shader harness, not measured on a
  physical Xbox 360 in this pass.
- The Python tool independently reimplements destination tiling, but uses the
  same published generic Xenos formula as the C++ path.
- No non-black runtime frame is claimed.
