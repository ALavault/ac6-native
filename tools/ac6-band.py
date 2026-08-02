#!/usr/bin/env python3
"""Compare two AC6 captures over the button band, and over the whole frame.

Why a band and not the frame mean: for roughly twenty cycles the investigation
measured a full-screen mean and concluded input did nothing. The dialog's
highlight occupies a small strip near the bottom of a 1280x720 frame, and a
real navigation moves far too few pixels to shift the frame mean above the
noise of an animating background. Measured on the band that actually contains
the highlight, the same press scores ~131 against a resting noise of 2-4 --
a signal-to-noise ratio of about forty, which is the difference between a
conclusive experiment and twenty wasted cycles.

Both numbers are printed. The band is the verdict; the full frame is context,
and a large full-frame delta with a small band delta means the background
animated and the highlight did not.

Usage:
    tools/ac6-band.py BEFORE.png AFTER.png [--region y0,y1,x0,x1]

Default region is the button band: y 590-660, x 600-1280.
"""

import argparse
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument(
        "--region",
        default="590,660,600,1280",
        help="y0,y1,x0,x1 of the band to measure (default: the button band)",
    )
    args = parser.parse_args()

    try:
        import numpy as np
        from PIL import Image
    except ImportError as exc:
        print(f"needs numpy and pillow: {exc}", file=sys.stderr)
        return 2

    y0, y1, x0, x1 = (int(v) for v in args.region.split(","))

    def load(path):
        return np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)

    before, after = load(args.before), load(args.after)
    if before.shape != after.shape:
        print(f"shape mismatch: {before.shape} vs {after.shape}", file=sys.stderr)
        return 2

    height, width = before.shape[:2]
    # A capture of the whole X root window is larger than the game's 1280x720
    # output, so the band coordinates would fall outside a letterboxed frame.
    # Clamp rather than fail: a clamped band is still comparable run to run, and
    # the effective region is printed so the reading is never ambiguous.
    y0c, y1c = max(0, min(y0, height)), max(0, min(y1, height))
    x0c, x1c = max(0, min(x0, width)), max(0, min(x1, width))
    if y1c <= y0c or x1c <= x0c:
        print(f"region {args.region} is empty in a {width}x{height} frame", file=sys.stderr)
        return 2

    full_delta = float(np.abs(after - before).mean())
    band_delta = float(
        np.abs(after[y0c:y1c, x0c:x1c] - before[y0c:y1c, x0c:x1c]).mean()
    )

    print(f"frame       {width}x{height}")
    print(f"band        y {y0c}-{y1c}  x {x0c}-{x1c}")
    print(f"band_delta  {band_delta:.3f}")
    print(f"full_delta  {full_delta:.3f}")
    # Calibration from cycle 421, restated so a reader does not have to find it:
    # ~131 is a real navigation, 2-4 is rest, and 4.4 was a candidate that
    # forced-write testing later eliminated. Treat anything under ~10 as noise.
    verdict = "MOVED" if band_delta >= 10.0 else "no change"
    print(f"verdict     {verdict}  (>=10 moves; navigation measured ~131, rest 2-4)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
