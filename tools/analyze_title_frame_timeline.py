#!/usr/bin/env python3
"""Frame timeline for a rendered demo probe run.

The title draw path prints one AC6_TITLE_DRAW_RESULT line per draw carrying the
frame's sha256, and AC6_TITLE_ORDER lines carry a monotonic batch number. The
draw lines have no tick stamp, so ordering comes from the batch number of the
nearest preceding AC6_TITLE_ORDER line in the interleaved stderr.

Two questions this answers, both about whether the title is an animated splash
or an interactive screen at a given point:

  timeline  -- the run of distinct frames, in order, with repeat counts
  diff      -- where two frames differ, as a bounding box and a coverage
               percentage; a small localized box is a blinking element, a
               full-frame box is a crossfade or moving picture
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

DRAW = re.compile(
    r"AC6_TITLE_DRAW_RESULT .*?rgb_nonzero=(\d+) .*?passed_samples=(\d+) sha256=(\w+)"
)
ORDER = re.compile(r"AC6_TITLE_ORDER batch=(\d+)")


def timeline(stderr_path: Path) -> list[dict]:
    """Distinct consecutive frames, each with the batch it first appeared at."""
    runs: list[dict] = []
    batch = 0
    for line in stderr_path.read_text(errors="replace").splitlines():
        if (order := ORDER.search(line)) is not None:
            batch = int(order.group(1))
            continue
        if (draw := DRAW.search(line)) is None:
            continue
        nonzero, samples, digest = draw.group(1), draw.group(2), draw.group(3)
        if runs and runs[-1]["sha"] == digest:
            runs[-1]["count"] += 1
            runs[-1]["last_batch"] = batch
            continue
        runs.append({
            "sha": digest,
            "short": digest[:8],
            "count": 1,
            "first_batch": batch,
            "last_batch": batch,
            "rgb_nonzero": int(nonzero),
            "passed_samples": int(samples),
        })
    return runs


def describe(image_path: Path) -> dict:
    from PIL import Image

    with Image.open(image_path) as handle:
        image = handle.convert("RGB")
        colors = image.getcolors(maxcolors=1 << 24) or []
    total = image.width * image.height
    colors.sort(reverse=True)
    return {
        "size": (image.width, image.height),
        "distinct_colors": len(colors),
        "top": [(rgb, n, round(100.0 * n / total, 2)) for n, rgb in colors[:5]],
    }


def diff(left: Path, right: Path) -> dict:
    from PIL import Image, ImageChops

    with Image.open(left) as a_handle, Image.open(right) as b_handle:
        a = a_handle.convert("RGB")
        b = b_handle.convert("RGB")
        if a.size != b.size:
            raise SystemExit(f"size mismatch: {a.size} vs {b.size}")
        delta = ImageChops.difference(a, b)
        box = delta.getbbox()
        # Any channel differing makes the pixel differ, so collapse the three
        # channels with a max-blend and count that band's nonzero histogram.
        channels = delta.split()
        merged = channels[0]
        for channel in channels[1:]:
            merged = ImageChops.lighter(merged, channel)
        changed = sum(merged.histogram()[1:])
    total = a.width * a.height
    result = {
        "bbox": box,
        "changed_pixels": changed,
        "changed_percent": round(100.0 * changed / total, 3),
        "frame_size": a.size,
    }
    if box is not None:
        width, height = box[2] - box[0], box[3] - box[1]
        result["bbox_size"] = (width, height)
        result["bbox_percent_of_frame"] = round(
            100.0 * width * height / total, 2)
        # A localized band reads as a discrete element (a prompt blinking);
        # a box covering most of the frame reads as a crossfade or a movie.
        result["verdict"] = (
            "localized" if result["bbox_percent_of_frame"] < 50.0 else "full-frame")
    else:
        result["verdict"] = "identical"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    t = sub.add_parser("timeline")
    t.add_argument("stderr_log", type=Path)

    d = sub.add_parser("describe")
    d.add_argument("image", type=Path)

    f = sub.add_parser("diff")
    f.add_argument("left", type=Path)
    f.add_argument("right", type=Path)

    args = parser.parse_args()
    if args.command == "timeline":
        runs = timeline(args.stderr_log)
        print(f"draws={sum(r['count'] for r in runs)} distinct_runs={len(runs)} "
              f"unique_frames={len({r['sha'] for r in runs})}")
        for index, run in enumerate(runs, 1):
            print(f"{index:3d}  {run['short']}  x{run['count']:<4d} "
                  f"batch {run['first_batch']}..{run['last_batch']}  "
                  f"nonzero={run['rgb_nonzero']}")
    elif args.command == "describe":
        for key, value in describe(args.image).items():
            print(f"{key}: {value}")
    else:
        for key, value in diff(args.left, args.right).items():
            print(f"{key}: {value}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
