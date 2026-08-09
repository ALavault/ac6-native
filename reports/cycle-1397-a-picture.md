# Cycle 1397 — a picture

## Qualification

- **No Ghidra run and no oracle pass.** This cycle ports no retail function.
- **Product C++ changed**: `demo_flight_view.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 46**, was 45.
- New capture `reports/mission01-native-captures/demo-flight-model/` — nine
  frames, a caption and a README.
- **No contract entry**, and it would be wrong to add one.

## What it shows

Nine frames of a thirty-second manoeuvre. The horizon is level, then banks; the
ground grid rolls with it. That is Ace Combat 6's own flight model responding to
a stick input, drawn.

## The file is named `demo_` on purpose

The campaign's rule is that no gameplay rule is invented. This file invents a
camera and a scene **deliberately**, so that the rules that are *not* invented
can be seen moving. Putting that in the filename is cheaper than relying on a
reader to remember it.

`caption()` is a function rather than a comment, and a test asserts it mentions
all three things — that the attitude is measured, that the camera and scene are
invented, and that the aircraft does not move. A caption that can be forgotten
will be.

## Two tests that are worth more than the picture

`a_command_the_model_discards_leaves_the_picture_alone` drives ninety frames of a
**quarter-degree** stick command and asserts the framebuffer is byte-identical.
That is the command setters' one-degree tolerance — contracted at cycle 1393 —
observed at the far end of the whole chain, in pixels.

`the_picture_follows_the_contracted_attitude` is its complement: ninety frames of
a real command must change the image. Together they say the chain is connected
and that it is connected *correctly*, which no single behaviour's differential
can.

## The thing I had to fix, and what it was

The first emission ran three seconds and every frame looked the same — 6531 to
6647 drawn pixels across nine frames.

That is not a bug. The base constructor's rate limits are **5.0, 1.4 and 5.4**,
read from the image at cycle 1377, and once the chain has scaled them the
aeroplane turns a few degrees per second. Three seconds is four degrees.

The honest response is a **longer** manoeuvre, not a larger one. Thirty seconds
gives a visible bank. Turning up the limits to make a better picture would have
been the first invented gameplay number in 1,397 cycles, and it would have been
invisible in the output.

## What the picture is not

- **Not Mission 01.** No retail geometry is loaded. That is the JV decision,
  still open, and it is the only thing between this and a recognisable scene.
- **Not retail's camera.** `0x82300C20` uses fourteen estimate instructions;
  cycle 1396 recorded the refusal to model them.
- **Not moving.** The position step depends on the same estimates.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 2 (1395, 1396) |
| implementation/integration spent on A3.3 | 1 (1397) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 46
tools/tests                           Ran 77 tests, OK
```

## Next

Live input. `retail_input_record` and its replay are contracted, and the demo
takes a `FlightStick` per frame — so a real controller can drive it through the
contracted binding layer instead of a script. That closes the loop the user asked
about: a capture of flight *with control*, rather than of a scripted manoeuvre.
