# A picture of the contracted flight model

Nine frames of a thirty-second manoeuvre: ten seconds of pitch command, ten of
roll, ten centred, sampled every 200 frames at 1/60 s.

## What is retail's, and measured

**The attitude, and only the attitude.** Every rule that turns a stick position
into the orientation drawn here comes from `retail_flight_session`, which
composes **twenty-five contracted behaviours**, each verified bit-for-bit against
the retail instructions by micro-execution:

- the three command setters (slots 12/13/14) with their one-degree tolerance;
- slot 30's first-order lag ramps and its quadratic axis curves;
- the rate servo, with three different biases and a gain switch that reads the
  opposite way from the obvious guess;
- the rotation angles, with one symmetric clamp and two one-sided ones;
- the three rotations of A3.1's transform kernel, in A3.1's order.

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
