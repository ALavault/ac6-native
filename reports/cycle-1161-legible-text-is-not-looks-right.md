# Cycle 1161 — legible text is not "looks right"

## What the decoder actually emits

`ac6-ntxr-extract` decodes the corpus to PPM so the output can be **looked at**,
which the corpus test cannot do: every assertion there is a count or a hash, and
a decoder emitting plausible noise would satisfy all of them. 334 wrappers
written, 12 refused, one copy of each duplicated tree.

A contact sheet of 45 distinct shapes shows, unambiguously:

- **Latin and Japanese font atlases with legible glyphs** — `ABCDEFGHIJKLMNOP`,
  kana and kanji, laid out in an even grid;
- **`B.C.S — BATTLE CONTROL SYSTEM`** and **`MACMILLAN HEAVY INDUSTRIES`**,
  which are Ace Combat 6's own in-universe names;
- aerial farmland, a tree billboard, cloud sprites, HUD panel frames, radar
  overlays, a tangent-space normal map with its characteristic violet, and an
  RGB test pattern.

## Why this is evidence and not vibes

This campaign's second anti-goal is *never declare parity by eye*, and cycles
1149 through 1152 spent themselves refusing exactly that. So it matters to say
precisely what this image does and does not do.

It is **not** a parity claim. Nothing here is compared against a retail frame,
no tolerance was pre-registered, and no capture from this decoder is offered as
visual parity — that stays where it was, with JG.

But legible text is not the same kind of evidence as "the atlas looks
intelligible", which is what `probe_ntxr_bc.py` rested on and what cycle 1152
found to be wrong in its alpha channel. A font atlas is closer to a checksum
than to a picture:

- the **Xenos `Tiled2D` swizzle** interleaves 32×32-block tiles with bank and
  pipe bits. Get it wrong and glyph rows land in the wrong tiles — you get
  shredded confetti, not slightly-off letters. Every letter is where it belongs
  across the whole atlas.
- the **8-in-16 byte swap** was the weakest link in the header, its only control
  negative and visual. Wrong endianness corrupts each block's colour endpoints;
  text would survive as shape but not as clean black-on-transparent.
- the **pitch** comes from `pad32(ceil(W/4))`. A wrong pitch shears the image
  progressively — the last rows would drift sideways. The grids are square to
  the bottom.

So this is a falsification test that the decode passed, on 45 shapes at once,
including non-power-of-two ones where the padding actually does something. That
is worth more than the phrase "visual check" suggests, and less than a
measurement. Both halves of that sentence are meant.

## What stays out of the repository

The decoded pixels are retail art. They are written to a local directory,
never committed and never redistributed, and `ac6-ntxr-extract` is a tool rather
than a test so nothing generates them in CI. `AGENTS.md` requires retail
containers to remain local; a decoded surface is the same content in a different
encoding, and the rule reads through.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```
