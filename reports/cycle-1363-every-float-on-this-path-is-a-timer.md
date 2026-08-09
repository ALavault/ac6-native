# Cycle 1363 — every float on this path is a timer

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus and `.pdata` were
  read.
- No product C++ changed, no contract changed.

## Where the command frame goes

`sub_82234040` — 742 instructions, `.pdata` agreeing — takes an object and a
float, receives the input command object as its **fifth** argument, fills the
command frame embedded at `object+0x838` via `0x82229250`, and immediately calls
`0x82227E10` with the object and the float it saved.

`0x82227E10` is 174 instructions, `.pdata` agreeing, and it reads six command
frame fields with fourteen floating-point operations. What it does with them:

```
f30 = the float
|axis at +0x838| compared against a threshold constant
|axis at +0x83C| likewise
[+0x870] += f30 ; compared against a limit
[+0x874] += f30 ; compared against a limit
[+0x878] += f30 ; compared against a limit
```

**Three more timers.** The float is accumulated and compared, exactly as in the
auto-repeat bank, and the axes gate the accumulation by magnitude.

## The finding, stated for the second time and now twice-supported

Cycle 1358 found the float on this tick accumulated into auto-repeat timers.
Cycle 1361 found the edge consumers triggering discrete actions from single bits.
This finds the analogue axes gating **another** timer bank.

**Every float on this path is a clock.** Nothing integrates a position, a
velocity or an orientation anywhere in the subtree rooted at `0x821CA908`.

That subtree is now well mapped — five contracted behaviours, `retail_input`
through `retail_slot_gather`, all with differentials — and it is the **input
command subsystem**. A3.2's integrator is not in it, and two independent lines of
evidence now say so.

## Which points somewhere specific

The flight orientation *is* already contracted: `retail_transform` reproduces
`0x822A1E80` and its three rotations. What has never been established is **what
drives its angles**.

Cycle 1330 read them: `f1 = [r30+0x18]`, `f2 = [r30+0x1C]`, and `0x822A2B50`
builds that argument block on its own stack from **its caller's floats**
(cycle 1331). So flight input enters at `0x822A2B50`'s callers — a bounded
question, on the side of the chain that already has a contract.

That is a better lead than anything downstream of `0x821CA908`, and it was reached
by exhausting the alternative rather than by guessing.

## Not established

- What the three timers at `+0x870`…`+0x878` measure.
- What `sub_82234040` does with the rest of its 742 instructions.
- Who calls `0x822A2B50`.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

`0x822A2B50`'s callers, and the floats they pass. It builds the argument block
whose `+0x18` and `+0x1C` become two of the transform's three angles, so its
callers are where a control input becomes an orientation — the question A3.2 has
been circling, approached from the contracted end.
