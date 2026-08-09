#!/usr/bin/env python3
"""Check that a capture's committed PNGs are the images its metrics describe.

The renderer writes PPM and records an FNV-1a hash of the colour buffer in the
metrics JSON beside it. The committed PNGs are `pnmtopng` conversions run BY
HAND, and nothing re-runs them. Cycle 1273 found the consequence: the far plane
changed, the metrics went from 4 markers to 29, and the PNGs kept showing the
4-marker world for a day. The contract pinned both -- correctly, and to
different states of the same render.

A capture whose image and metrics disagree is worse than no capture. The image
is what a reader looks at; the metrics are what the gate audits. Nothing
connected them until this.

THE CHECK. The hash is reproducible from the PNG. `NativeRenderTarget::readback`
walks the colour buffer as 32-bit values, seeded 1469598103934665603 and
multiplied by 1099511628211 -- FNV-1a. The PPM drops alpha and the renderer's
alpha is 0xFF, so decoding the PNG to RGB and rebuilding 0xFFRRGGBB reproduces
the stored value exactly. Verified against the JF capture: hud-live and
hud-debrief both match to the digit.

It is an equality on a 64-bit hash of every pixel, so it cannot pass by
coincidence, and it fails the moment an image is regenerated without its metrics
or the reverse.

usage: audit_capture_images_match_metrics.py CAPTURE_DIR [CAPTURE_DIR...]
exit 0 when every PNG that has a matching metrics entry agrees with it.
"""

import glob
import json
import os
import sys

FNV_OFFSET = 1469598103934665603
FNV_PRIME = 1099511628211
MASK = (1 << 64) - 1


def colour_hash(path):
    try:
        from PIL import Image
    except ImportError:
        return None, "Pillow is not installed; cannot decode PNG"
    try:
        image = Image.open(path).convert("RGB")
    except OSError as exc:
        return None, str(exc)
    # tobytes() rather than getdata(): the latter is deprecated in Pillow 14,
    # and a flat bytes walk is the same arithmetic without the per-pixel tuple.
    raw = image.tobytes()
    digest = FNV_OFFSET
    for i in range(0, len(raw), 3):
        colour = 0xFF000000 | (raw[i] << 16) | (raw[i + 1] << 8) | raw[i + 2]
        digest = ((digest ^ colour) * FNV_PRIME) & MASK
    return digest, None


def declared_hashes(document, prefix=""):
    """Map every color_hash in the document to the key path that carries it."""
    found = {}
    if isinstance(document, dict):
        for key, value in document.items():
            if key == "color_hash" and isinstance(value, int):
                found[prefix.strip(".")] = value
            else:
                found.update(declared_hashes(value, prefix + "." + key))
    elif isinstance(document, list):
        for index, value in enumerate(document):
            found.update(declared_hashes(value, "%s[%d]" % (prefix, index)))
    return found


def main() -> int:
    directories = sys.argv[1:]
    if not directories:
        print("usage: audit_capture_images_match_metrics.py CAPTURE_DIR [...]")
        return 1

    compared = 0
    mismatched = 0
    unmatched = 0

    for directory in directories:
        metrics_files = glob.glob(os.path.join(directory, "*.json"))
        declared = {}
        for path in metrics_files:
            try:
                with open(path, encoding="utf-8") as handle:
                    declared.update(declared_hashes(json.load(handle)))
            except (OSError, ValueError) as exc:
                print("  UNREADABLE METRICS  %s  %s" % (path, exc))
                return 1
        if not declared:
            print("  NO color_hash IN  %s" % directory)
            continue

        for png in sorted(glob.glob(os.path.join(directory, "*.png"))):
            stem = os.path.splitext(os.path.basename(png))[0]
            # "hud-live.png" carries the hash recorded under the "live" key.
            key = next((k for k in declared if k and k.split(".")[-1]
                        and stem.endswith(k.split(".")[-1])), None)
            if key is None:
                unmatched += 1
                print("  NO METRICS ENTRY  %s  (no color_hash key matches its "
                      "name; the image is unchecked)" % png)
                continue
            actual, error = colour_hash(png)
            if actual is None:
                print("  UNDECODABLE  %s  %s" % (png, error))
                return 1
            compared += 1
            if actual != declared[key]:
                mismatched += 1
                print("  STALE  %s" % png)
                print("      metrics %-24s declares %d" % (key, declared[key]))
                print("      the image itself hashes to %d" % actual)

    status = "pass" if mismatched == 0 else "fail"
    print("capture_images_match_metrics=%s compared=%d mismatched=%d unmatched=%d"
          % (status, compared, mismatched, unmatched))
    return 0 if mismatched == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
