# Cycle 1417 — a checker that checked nothing

## Qualification

- **No Ghidra run and no oracle pass.** Product C++, one tool, one capture.
- ctest stays **51**. **No contract entry.**
- `reports/mission01-native-captures/demo-bare-flight/metrics.json` is new.

## The loose end cycle 1416 named

Cycle 1416 shipped a capture and observed that
`tools/audit_capture_images_match_metrics.py` did not cover it. Pointed at the
new directory, the tool said:

```
capture_images_match_metrics=pass compared=0 mismatched=0 unmatched=0
```

**A pass, having compared nothing.** Two holes, and the second is worse:

- a directory whose images have no matching metrics key reported
  `pass compared=0 unmatched=4` — `p2-native-hud` has been doing that all along;
- a directory with **no metrics file at all** bailed before its images were even
  counted, so the unmatched figure was zero too. Nothing in the output
  distinguished it from a directory that had been verified.

This is the shape cycle 1414 found in the harness calibration — a checker
reporting success while checking nothing — and here it can be made to fail
outright, because a capture with images and no metrics to check them against is
exactly what this tool exists to refuse.

```
capture_images_match_metrics=fail compared=0 mismatched=0 unmatched=7
  NOTHING WAS COMPARED: 7 image(s) carry no metrics to check them against,
  so this directory is unverified rather than correct
```

## And the capture is now genuinely checked

`--emit-flight` writes `metrics.json` with a `color_hash` per still, computed
**from the framebuffer** — not from the PNG.

That direction is the whole value. The `.png` files in every capture directory
here are `pnmtopng` conversions run by hand and nothing re-runs them; hashing the
image to produce the number it is then checked against would be circular and
would verify nothing. Hashing the framebuffer and comparing the *decoded PNG*
against it covers the conversion step, which is the one nothing else guards and
the one cycle 1273 was caught by.

```
capture_images_match_metrics=pass compared=7 mismatched=0 unmatched=0
```

## An open defect this cycle found and cannot fix

Run over every capture directory:

```
capture_images_match_metrics=fail compared=6 mismatched=3 unmatched=24
  STALE  p6-native-hud/hud-failure.png   metrics declares 9912235326003257103, image hashes 8445613777507884815
  STALE  p6-native-hud/hud-live.png      metrics declares 11411122341720436440, image hashes 6467555704147619544
  STALE  p6-native-hud/hud-success.png   metrics declares 10721361994103336150, image hashes 2680219722504940758
```

**`analysis/contracts/mission01-native-gate-v2.json` cites all three PNGs and
`native-hud-debrief.json` as evidence, pinned by hash.** So a contract pins a set
of images and a metrics file that describe different states — cycle 1273's shape,
already committed and cited.

What was checked before saying so, because two cycles running have now found the
instrument at fault rather than the artefact:

- **Pillow is installed**, so the decode is not falling through to a stub;
- the tool's own docstring says it verified `hud-live` "to the digit" when it was
  written, and `git log` shows the directory unchanged since a single commit —
  so one of the two claims moved and it was not the images;
- **three of the six comparisons elsewhere still match**, so the hash and the
  decode work.

I cannot regenerate these: the directory holds no `.ppm` sources, and the images
come from a native session run. It is recorded as an open defect rather than
quietly renormalised, because re-pinning the hashes to the current images would
make the contract self-consistent and still wrong.

## Not established

- Which of the p6 pair is stale — the images or the metrics. Settling it needs
  the session that produced them.
- The other 24 unchecked images across `p2-native-hud` and `p7-current-main`.
  They are now *reported* as unchecked rather than silently passing, which is the
  part this cycle could fix.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
capture_images_match_metrics          demo-bare-flight: pass compared=7
                                      whole tree: fail, 3 stale + 24 unchecked -- OPEN
```

## Next

**Thread B, the visible world.** Thread A has taken the flight model as far as
the estimate boundary allows: the aeroplane flies under retail's own arithmetic
and what remains chosen there is a heading and a speed that cannot be recovered.

The plan lists eight gaps and two decisions for Thread B, and the first decision
is the one that gates the rest: **how a model is loaded**. Cycle 1246 established
that retail resolves assets by integer id through registries and never walks the
directory, so porting an FHM directory walk would be porting something the game
does not do — and an offline extraction is a manifest under another name, which
JF exists to eliminate.

The first bounded step is reconnaissance, not code: confirm the asset tree is
present and readable at
`reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009`, and read what
`NdxrContainer` actually refuses to serve (`retail_ndxr_container.h:284`, gap 3 —
`bytes_`/`size_` private with no accessor). Those two facts decide whether the
first port is a reader or an accessor.
