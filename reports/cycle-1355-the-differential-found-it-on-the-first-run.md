# Cycle 1355 — the differential found it on the first run

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass.** `0x82211C10` was micro-executed 38 times on synthetic state.
- **Product C++ changed**, and the **twelfth behaviour** is under contract.

## What the differential caught that the reading did not

Its first run failed on fifteen of nineteen values, all the same way: **the layer
stored nothing at all**.

```
0x82211D50   fcmpu cr6,f13,f10
0x82211D54   beq   cr6,0x82211DD0
```

When the processed value is exactly zero, the function branches straight to the
loop increment. No value, no step, **no mask bit** — and the caller's arrays keep
whatever the previous frame left there.

Cycles 1353 and 1354 read this function twice and both missed it. The port
returned a value unconditionally, which would have **cleared a slot retail
preserves**, on every idle axis, on every frame. An idle axis is the most common
input in the game.

The port now returns `std::optional`, and the header says why an empty one is not
zero.

## And a second correction, to my own model

Six cases then failed by one or two ulps at `0.3` and `0.2500001`. Retail's
`fabs`, `fsubs` and `fmuls` are **single precision throughout**; my Python model
subtracted in double. The model was wrong, not the port.

Rounding every step to `float32` closes it: **38 of 38, bit for bit.**

## The layout confirmed itself

`0x82211DF8` advances the player block by **912** bytes per player. A descriptor
is 24 bytes at `player + 24*(bit + 6)`, so bits 0..31 need `24*37 + 24` = **912**.

Two numbers read from two different functions for two different reasons, and they
are the same number. That is why the capsule's layout is not a guess.

The record bit driven is **17**, whose slot is `(17+3)*4 = 0x50` — LY in the
contracted record layout. The capsule drives the slot a stick actually reaches.

## The twelfth behaviour

`retail_input_binding`, with all four evidence kinds. The `microexec` one is the
kind cycle 1354 refused to proceed without:

```
mission01_final_gate (playable-v1)   JF=pass open=none, 12 behaviours
ctest                                100% passed, 0 failed out of 31
contract_addresses                   pass, 169 cited, 169 supported
contract_derivations                 pass, 30 behaviours, 0 gaps
input_binding_microexec              pass, 38 cases, 0 divergences
tools/tests                          Ran 72 tests, OK
```

The gate refused the entry once, for a derivation that did not cite `0x82069B28`
— the `−1.0` constant. Citing it in the header fixed it. That checker has now
caught its author four times.

## Not established

- What consumes the four output arrays.
- Why the descriptor index starts at `+6`.
- Whether all 32 output slots are used.

## Next

The four output arrays at `this+0xE58`, `+0xED8`, `+0xF58` and `+0xFD8` — two
carry the outputs and two carry the deadzone and threshold, which the layer
copies out per slot. What reads them is the next link toward the flight model,
and it is a bounded question: they are fields of one object at fixed offsets.
