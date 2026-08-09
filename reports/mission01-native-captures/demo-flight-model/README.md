# A picture of the contracted flight model

Nine frames of a thirty-second manoeuvre: ten seconds of pitch command, ten of
roll, ten centred, sampled every 200 frames at 1/60 s.

## What is retail's, and measured

**The whole path from the controller record to the orientation.** Regenerated at
cycle 1408 through the contracted player path -- a controller record, the
contracted binding layer, `0x82227E10`'s five increments, and the five clamped
accumulators -- composing **twenty-seven contracted behaviours**, each verified
bit-for-bit against the retail instructions by micro-execution:

- the three command setters (slots 12/13/14) with their one-degree tolerance;
- slot 30's first-order lag ramps and its quadratic axis curves;
- the rate servo, with three different biases and a gain switch that reads the
  opposite way from the obvious guess;
- the rotation angles, with one symmetric clamp and two one-sided ones;
- the three rotations of A3.1's transform kernel, in A3.1's order;
- and, since cycle 1408, the input path itself: `build_input_record`,
  `apply_input_binding`, the five increments and the five accumulators.

**The frames are byte-identical to the previous version, and that is a result
rather than a failure.** The earlier capture drove the AI's setter interface with
an increment of 1.0; this one drives the player's accumulators with a binding
output of 0.72. Both saturate the accumulator's clamp within two frames, and the
frames are sampled every 200, so from the first sample on the attitude is the
same. At frame 0 the difference is a rotation of about 2.8e-05 radians --
sub-pixel at 480x270. Two different retail interfaces, one physics.

The aircraft's rate limits are the base constructor's own defaults — 5.0, 1.4 and
5.4, read from the image at cycle 1377. They are a few degrees per second, which
is why the manoeuvre is thirty seconds and not three.

## What is invented, and named

- **the camera.** Retail's gameplay camera is `0x82300C20`; cycle 1396 counted
  fourteen estimate instructions in it (`vrefp` ×6, `vrsqrtefp` ×8), whose exact
  results are a property of the console. This campaign refuses to approximate
  them, so the camera here is mine.
- **the scene.** A ground grid and a horizon ring. No retail geometry is loaded
  and none is claimed; loading it is the JV decision, still open.
- **which basis row is which axis.** Nothing established that row 0 is right,
  row 1 up and row 2 forward. It is assumed here to have something to draw.
- **which controller axis feeds which input field.** Cycle 1404 established that
  two of the six fields come from the binding layer's first two outputs; which
  binding slots those are, and what fills the other four, is not established.
- the field of view, the altitude, the grid spacing, the colours.

## What is absent

**The aircraft does not move.** Its position step is `0x823042D0`, which depends
on the same two estimate instructions, so cycle 1383 put it out of reach. What
these frames show is an aeroplane changing attitude in place.

## Reproducing

```
cmake --build reconstruction/ace-combat-6/build
./reconstruction/ace-combat-6/build/ac6-demo-flight-view-tests --emit-frames DIR
```

The `.png` files are `pnmtopng` conversions of the `.ppm` the tool writes, run by
hand — the same arrangement as every other capture directory here, and the same
caveat: nothing re-runs them.
