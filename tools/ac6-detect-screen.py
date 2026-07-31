#!/usr/bin/env python3
"""Say which known AC6 screen a capture shows.

Navigation kept failing because the driver could not tell what was on screen.
"Did the frame change?" is not a state detector -- a playing cutscene changes
every frame, which made two navigation attempts report success while sitting in
the intro (cycles 391-392).

This matches a capture against a template taken from a REFERENCE capture of the
save screen: the YES/NO button band, which no other screen shows.

Usage: ac6-detect-screen.py <capture.png> [template.npy]
Prints "save-screen <score>" or "other <score>"; exit 0 when matched.
"""
import sys
import numpy as np
from PIL import Image

cap = sys.argv[1]
tpl_path = sys.argv[2] if len(sys.argv) > 2 else \
    "/home/lavaulta/.claude/jobs/c4f079d5/tmp/savetpl.npy"

tpl = np.load(tpl_path).astype(np.int16)
img = np.asarray(Image.open(cap).convert("RGB"), dtype=np.int16)
crop = img[590:660, 620:1270]
if crop.shape != tpl.shape:
    print("other shape-mismatch")
    sys.exit(1)

# Mean absolute difference over the button band. The band is high-contrast
# (light buttons on dark panel), so a wrong screen scores far above a right one.
mad = float(np.abs(crop - tpl).mean())
if mad < 25.0:
    print(f"save-screen {mad:.1f}")
    sys.exit(0)
print(f"other {mad:.1f}")
sys.exit(1)
