# First visual capture of the demo's readback: solid black, one color, confirmed by pixel count

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `probe --until frontend
--max-ticks 4000 --backend vulkan` under a real (not headless) Xvfb
display, `--input-at 3000,16,...` (correctly-timed START), no oracle.

## What this adds

No prior report in this campaign ever produced an actual image of the
demo's own rendered output — every characterization of "the black frame"
rested on a SHA256 hash (`0b150fd3...`) matching across runs, never a
directly viewed picture. Added a small opt-in dump,
`AC6_DEMO_DUMP_READBACK_PPM=<path>`, at the exact point
`commit_reached_guest_present` (`xenos_guest_present_join.hpp`) computes
`guest_linear` — the same linear RGBA8 buffer already being hashed into
`guest_linear_rgba8_sha256` — writing it out as a plain PPM (P6, alpha
dropped) so it converts with `pnmtopng` the same way every other committed
capture in this repo already does.

## The result

`normal_readback_sha256=0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366`
— the long-tracked hash — confirmed once more, but this time with the
actual pixels alongside it. Converted and inspected directly: **1280×720,
exactly one distinct color across all 921,600 pixels, `(0,0,0)`.** Not
"mostly black" or a hash coincidence — the readback this campaign has
been reasoning about for the whole demo-render-chain investigation is
provably, exactly, uniformly black.

## Reading

This doesn't change any conclusion already on record — every report that
cited `0b150fd3...` as "the black readback" was already correct. What it
adds is direct confirmation that the hash means what this campaign has
assumed it means, and gives the campaign, for the first time, an actual
image artifact rather than only a fingerprint. No new finding about *why*
the frame is black — that remains the mission-scoped `CX360UnitManager`
render gate (`eab92d66` and the demo-render-chain memory's standing
answer), unaffected by this report.

## Not established

- Whether a frame captured at some other tick (mid-attract-loop, before
  the press, or much later) ever differs — not swept; this campaign's own
  prior work (`AC6_DEMO_SWG_DRAW_CALL_CHAIN_RUNS_CLEAN_FOR_FIVE_HOPS.md`)
  already established the renderer's CPU-side draw logic runs
  continuously regardless, so a black readback at every sampled point is
  the expected outcome, not swept exhaustively here.
- No on-screen X11 window is ever created by the `vulkan` backend even
  under a real Xvfb display (`xwininfo` showed zero children on the root
  window) — the render target is an off-screen resolve, not a presented
  swapchain surface a screenshot tool could capture directly. This is why
  the dump reads the same in-memory buffer the SHA256 already covers,
  rather than an X11 screenshot.

## Gates

New env var `AC6_DEMO_DUMP_READBACK_PPM`, opt-in, unset by default, a
plain file write gated behind `getenv` — no behavior change when unset.
Native gate JF, demo `ctest` (26/26), and both contract audits verified
below before commit.
