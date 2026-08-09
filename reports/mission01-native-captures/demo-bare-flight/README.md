# Bare flight — thirty seconds, and the aeroplane moves

`bare-flight.mp4`, 900 frames at 30 fps, from a 1800-frame run at 1/60 s.
Seven of those frames are kept alongside as `.ppm`/`.png`.

The stick: ten seconds of pitch, ten of roll, ten centred. The aircraft climbs
from 1000 units to 1489 and travels 7478 units downrange.

## What is retail's, and measured

**The attitude and the position, both.** This is the first capture in the
campaign where the aeroplane moves, and the movement is not a rendering trick:
`integrate_flight_position` (`0x82303110`) is a contracted behaviour and it is
what advances the position every frame — its `1/3.6` km/h→units scale from
`0x82069B40`, its `10.0` floor on the vertical component from `0x82003214`, and
its fused `fmadds` steps.

Twenty-nine contracted behaviours compose the frame, each verified bit-for-bit
against the retail instructions by micro-execution:

- the input path — `build_input_record`, `apply_input_binding`,
  `route_flight_input_fields`, the five increments and the five clamped
  accumulators;
- slot 30's first-order lag ramps and its quadratic axis curves;
- the rate servo, with its three biases and its gain switch;
- the rotation angles and the three rotations of A3.1's transform kernel, whose
  trigonometry is now `XMScalarSinCos` itself (`0x8209CB70`, cycle 1412) and not
  the host library;
- and the position integrator above.

**The integrator is exactly right, and there is a number for it.** Flown with no
pitch command at all, thirty seconds at 900 km/h gives `at72 = 7499.899`.
900 ÷ 3.6 × 30 = 7500. That is retail's own scale doing the arithmetic.

**The 10.0 floor is retail's and it fires.** Flown with the pitch command at
`+1.0` instead, the aircraft descends from 1000 and stops at `at68 = 10.000`
exactly — a minimum altitude nobody in this campaign chose.

## What is invented, and named

- **the heading and the speed.** This is the one that grew. Cycle 1415 read the
  integrator's window: its three rates come off stack slots that a vector
  normalise fills, and that normalise seeds on `vrsqrtefp` (`0x823034CC`) and
  `vrefp` (`0x823034FC`) — estimate instructions specified only to a relative
  accuracy bound, which this campaign refuses to approximate. So retail supplies
  a **unit direction** there and `[model+32]` supplies the **speed**, and neither
  is obtainable. Here the direction is basis row 2 and the speed is 900 km/h.
  **The heading is chosen; the flying of it is not.**
- **the gravity bias is passed as 0.0.** Retail's is `f25*f24` and `f24` is
  unresolved. Passing retail's `f25` alone would assume `f24 = 1` *and* would
  sink the aircraft, because the lift that balances gravity is not in the
  contracted chain at all.
- **the camera.** Retail's gameplay camera is `0x82300C20`; cycle 1396 counted
  fourteen estimate instructions in it. Not reproduced.
- **the scene.** A ground grid and a horizon ring, and the grid is snapped to the
  eye so a moving aircraft does not fly off a finite one. No retail geometry is
  loaded and none is claimed; loading it is the JV decision, still open.
- **which basis row is which axis.** Nothing established that row 0 is right,
  row 1 up and row 2 forward.
- the field of view, the grid spacing, the colours.

## One thing this capture taught

Driven with pitch **targets** of `+0.8` and `-0.8`, the two runs produced
byte-identical trajectories. Retail's setters compare `|current - target|`
against one degree and, when that passes, store the target and add the
**increment** — so the target only decides whether a command is taken at all,
and the increment is what flies the aeroplane. The loop drives `increment12`
now, and the sign works: `-1.0` climbs to 2304, `+1.0` descends to the floor.

## Reproducing

```
cmake --build reconstruction/ace-combat-6/build
./reconstruction/ace-combat-6/build/ac6-demo-flight-view-tests \
    --emit-flight DIR 2 -0.4
ffmpeg -y -framerate 30 -i DIR/flight-%05d.ppm -c:v libx264 -pix_fmt yuv420p \
    -crf 20 -vf "scale=960:540:flags=neighbor" bare-flight.mp4
```

The fourth argument is the pitch **increment**. The `.png` files are `pnmtopng`
conversions run by hand, the same arrangement as every other capture directory
here and with the same caveat: nothing re-runs them.
