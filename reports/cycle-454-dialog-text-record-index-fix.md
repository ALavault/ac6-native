# Cycle 454 — invalidated dialog text record hypothesis

## Qualified scope

- Target: Ace Combat 6 PAL, Xenon PPC big-endian, image base `0x82000000`.
- Analysis XEX SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Runtime `default.xex` SHA-256: `c6ca3556c3a7278d42326bbd894d0740203824525f1ebc6a84e0730126d3b2bf`.
- Generated recompiler output was not edited.

## Invalidated hypothesis

The repeated-input diagnostic route eventually observed key `M70000_122` and
record `1088`, whose declared length is zero. Adjacent record `1089` declares
17 glyphs at the same packed-data offset. Redirecting record 1088 to 1089 made
the native runtime display `Save Replay Data?`.

That result does **not** establish an off-by-one lookup. It establishes that the
repeated input sequence advanced to a later replay prompt and that the adjacent
record is renderable. The shared glyph offset is consistent with an intentional
zero-length packed record. `Save Replay Data?` is unrelated to the first screen
reported after the title.

Confidence: record fields and rendered text are `dynamic/confirmed`; the
off-by-one interpretation is **refuted**.

## Disposition

The exact-key override, helper, and unit test were removed. No native behavior
change from this hypothesis remains. The locale hypothesis also remains closed
by the user's A/B observation: changing locale did not affect the symptom.

The unresolved boundary is now narrower: reproduce the first post-title screen
with a minimal, timed input sequence, identify its own text key/state, and only
then change native behavior.

## Evidence retained

The captures showing `Save Replay Data?` are retained only as negative-control
evidence that the text renderer can display a valid neighboring record. They
are not acceptance evidence for the reported dialog.
