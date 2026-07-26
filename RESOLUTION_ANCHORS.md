# Ace Combat 6 - initial resolution and viewport anchors

Date: 2026-07-14

Scope: address-stable static evidence from the XEX memory image. Names remain
working descriptions; the Ghidra database has not been mass-renamed.

## Full-screen viewport path

`0x821dcfe8` consumes six consecutive 32-bit fields and forwards them to
`0x821da938` as four integer-valued coordinates followed by two floats. This is
the exact 24-byte layout of an Xbox 360 / D3D9 viewport:

```text
u32 x, y, width, height;
float min_depth, max_depth;
```

The compile-checked host representation is
`source/recovered/render_state_types.h`.

Five statically recovered callers reach the wrapper:

- `0x820af548`
- `0x820af5d8`
- `0x821382c0`
- `0x8234e8f0`
- `0x821da5c0`

Both `0x820af548` and `0x821382c0` build `(0, 0, 0x500, 0x2d0, 0.0, 1.0)`, hence
a full-resolution `1280 x 720` viewport, before calling `0x821dcfe8`. The depth
values were read as big-endian floats from globals `0x820542b8` and
`0x820542bc` in the XEX memory image.

`0x820af5d8` instead binds resources at `0x8293b7e0` / `0x8293b7e4` and builds
`(0, 0, 0x280, 0x168, 0.0, 1.0)`: a `640 x 360` half-resolution viewport.
`0x820aebc8` allocates the corresponding pair of `640 x 360` surfaces through
`0x821e0e88`. This is direct evidence that the native rendering graph mixes
full- and half-resolution passes.

`0x82146c78` allocates two `640 x 1440` surfaces through the same allocator.
Their dimensions and a vertical offset of 720 suggest a stacked surface or
atlas, but that role and the opaque format words are not yet proven.

`0x82249258` first invokes a virtual method at slot `+0x10` with a dynamically
positioned square, then restores `(0, 0, 0x500, 0x2d0, 3)` and
`(0, 0, 0x500, 0x2d0, 1)`. This corroborates full-resolution viewport state in
an effect-like path; the final selector and ownership are not yet named.

## Shader-visible dimensions

`0x82105aa8` writes two dimension pairs to the named shader parameter
`ACE_vCommonParam2` through `0x82334178`: first `(208.0, 144.0)`, then
`(1280.0, 720.0)`. The values are verified directly in the big-endian memory
image. This makes the function a high-priority modernization boundary: shader
dimensions are pass-dependent and cannot safely be replaced by one global host
window size. The exact meaning of the small pair is not yet classified.

Run `scripts/analyze_resolution_anchors.py` to validate these exact fragments
against the re-agent bridge export and regenerate
`reports/resolution-anchors.json`. The script fails rather than silently
carrying an anchor forward when a later export changes.

## Platform video anchors

- Import `XGetVideoMode` is at `0x823d735c`; its single direct caller is
  `0x82339bf0`. The caller reads a float from the returned structure and divides
  it by a synchronized counter. This is likely timing/refresh logic, not enough
  evidence for a resolution setter.
- Import `VdGetCurrentDisplayInformation` is at `0x823d6f1c`; no direct caller
  was recovered in the current static graph.
- RTTI strings identify `NU::Draw::RequestViewport` at `0x82676494` and
  `ACE6::CAce6HudDisplay` at `0x8268ee60`, but neither string currently has a
  direct code reference in the export.

## Modernization boundary

The native renderer is demonstrably 1280 x 720 at these call sites. The first
portable change should route viewport construction through a host-output policy
while retaining a strict 1280 x 720 compatibility mode. Projection matrices,
render targets, scissor rectangles, post-processing buffers, HUD coordinates
and texel-size constants must be classified before arbitrary output dimensions
can be claimed safe.

No aspect-ratio independence, UI safety, render parity or working port is
claimed yet. VMX128 instructions remain incompletely decoded by the current
Ghidra language, so vector-heavy functions require independent corroboration.
