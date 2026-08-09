# Cycle 1354 — a port, and a contract entry withheld

## Qualification

- **No Ghidra run and no oracle pass.**
- **Product C++ changed**: `retail_input_binding.h`, `retail_input_binding.cpp`,
  `retail_input_binding_tests.cpp`. ctest is **31**.
- **No contract entry**, deliberately — see below.

## The port

`0x82211C10`'s binding layer is in the product. One binding takes a record value
and a 24-byte descriptor and produces two floats:

```
value:  f = |v| - deadzone ; f < 0 ? 0 : min(f * scale, 1) ; sign of v restored
step:   0 inside the deadzone, +-1 beyond the threshold, THE INPUT ITSELF between
```

Every deadzone, scale and threshold comes from the descriptor. Nothing is a
constant in the code, because nothing is a constant in retail.

## Writing the port corrected yesterday's description

Cycle 1353 called the second output a *"three-state sign"* — zero or ±1. Following
the branch targets to write the code shows a **middle band**: between the deadzone
and the threshold, `step` is the raw input, untouched.

Three regions, only two of them constant. The test that says so is
`step passes the input through between deadzone and threshold`, and the control
below proves it can fail.

## The detail that makes it faithful

`fsel(a, b, c)` compares against **+0.0**, and `-0.0 >= 0.0` is true — so negative
zero takes the *positive* branch.

That is not trivia. Cycle 1323 measured that **an idle axis leaves negative zero
in the record**, so every idle binding on every frame arrives here as `-0.0` and
must leave as `+0.0`. A port written with `std::signbit` gets this backwards on
the most common input in the game.

## Two controls, and both bite

```
signbit instead of >= 0.0     2 failures, both on -0.0
step as a pure sign           4 failures, all in the middle band
```

The first fails on exactly the negative-zero cases and nothing else. The second
fails at `0.5`, at exactly the deadzone and at exactly the threshold — the three
points where a two-region rule and a three-region rule differ.

## Why there is no contract entry

The plan is explicit: *"Aucune behaviour de vol au contrat sans différentiel
`microexec`, même au prix d'un blocage."* No flight behaviour enters the contract
without a micro-execution differential, even if that blocks.

This behaviour has static evidence, a native test and a derivation. It has **no
microexec**, so it does not enter the contract, and the eleven behaviours stand
unchanged. The rule exists because "1:1" is otherwise undemonstrable, and the
first time it costs something is the first time it means anything.

`0x82211C10` is differentiable: `r3` a player block, `r4` a record, `r5` a mask
word and `r6..r9` four arrays — all synthetic, all buildable. That is the next
cycle's work and the entry goes in with it.

## Not established

- What consumes the four output arrays.
- Why the descriptor index starts at `+6`.
- Whether all 32 output slots are used.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 31
tools/tests                          Ran 72 tests, OK
```

## Next

The differential: `0x82211C10` micro-executed on a built player block and a
record whose contents the contracted producer would produce, compared against
`apply_input_binding` value by value. Then the twelfth behaviour.
