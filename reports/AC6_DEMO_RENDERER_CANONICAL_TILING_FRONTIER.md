# AC6 PAL demo renderer: canonical tiled guest writeback

## Scope

This change closes the **linear RGBA8 ↔ reached Xenos tiled allocation**
transport used by the PAL demo's qualified `1280×720` `XE_SWAP` destination.
It does not claim that the current normal draw is non-black, that the EDRAM
sample packing is correct for non-zero values, or that a host window displayed
the resulting frame.

The qualified destination contract remains:

- physical address: `0x1374A000`;
- width: `1280`;
- height: `720`;
- pitch: `1280` pixels;
- format: reached raw format `6`;
- linear pixel bytes: `0x384000`;
- tiled allocation extent: `0x398000`;
- non-pixel padding: `0x14000` bytes.

The raw register and packet provenance is retained in
`analysis/demo/ac6-demo-copy-resolve-profile-v1.json`.

## Previous writeback

`commit_reached_guest_present` copied each four-byte pixel directly from the
GPU-produced tiled payload into the guest allocation. It then untiled the guest
allocation and compared the resulting linear SHA-256 with the resolve result.

That validated the final pixels but trusted the GPU compute shader and the CPU
writeback loop to agree on the same tiled placement. It also left no reusable
inverse of `untile_reached_rgba8` for a future non-black CPU oracle.

## New contract

`tile_reached_rgba8` is the exact inverse over all `921,600` reached pixel
coordinates. It writes only the four bytes addressed by each pixel and leaves
all `81,920` padding bytes untouched.

The guest-present path is now:

```text
GPU tiled payload
→ CPU untile to 1280×720 RGBA8
→ verify linear SHA-256
→ load existing guest allocation
→ CPU tile the verified linear pixels into that allocation
→ store and reread guest allocation
→ CPU untile again
→ verify the same linear SHA-256
→ publish the audit screencap
```

The header-only `canonicalize_reached_tiled_writeback` helper performs this conversion transactionally. A corrupt, malformed or mismatched GPU tiled payload is rejected before any guest byte is modified.
The guest allocation's padding remains guest-owned and is not replaced by
Vulkan canaries or zero-fill.

## Tests

The targeted C++20 tests cover:

- exact reached offsets, including `(1279,719) → 0x39777C`;
- one-to-one mapping of all `921,600` pixels;
- a non-black deterministic RGBA pattern;
- full tile/untile equality;
- preservation of all `0x14000` padding bytes;
- the known all-zero linear SHA-256;
- out-of-bounds coordinates and wrong buffer sizes;
- the complete guest-present canonicalization with a stub guest allocation;
- rejection of malformed and false linear digests before writeback;
- byte-identical guest state after every rejected canonicalization.

Compilation uses `-std=c++20 -UNDEBUG -Wall -Wextra -Wpedantic -Wconversion
-Wshadow -Wdouble-promotion`.

## Remaining frontier

This change qualifies **destination transport**, not the source conversion.
The next renderer boundary is:

```text
non-black 640×360 normal readback
→ qualify RGBA channel/endian representation
→ materialize the four 2×2 MSAA samples in Xenos EDRAM layout
→ run the reached copy/convert shader
→ compare GPU copy output with an independent CPU oracle
→ permit non-black guest writeback
```

Until that source-side contract is closed, the black-only EDRAM materializer
and resolve qualification must remain fail-closed.

## Validation limits

No new PAL demo run, Xenia run, complete CMake build, or visible screencap is
claimed by this report. The tests validate the bounded CPU transport and its
integration point.
