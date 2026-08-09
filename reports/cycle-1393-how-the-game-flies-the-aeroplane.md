# Cycle 1393 — how the game flies the aeroplane

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_flight_command.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 44**, was 43.
- New tool `tools/audit_flight_command_microexec.py`, new artefact
  `analysis/flight/flight-command-microexec.tsv`.
- **Contract: the twenty-fifth behaviour**, `retail_flight_command`.

## The decision this cycle takes

The demo question is answered by the same work either way, so it is not a fork:
the gap between a contracted flight model and a flyable one is that **nothing
connects an input to a command**. This cycle closes the model's side of it.

Writers of `[+36]`, `[+40]`, `[+44]` are **487 functions** unbounded and **nine**
inside this class family — of which three are one-store setters on the base
vtable, inherited by every branch. Those are the API.

## The command input API

`0x82281608`, `0x822816C0`, `0x82281778` — slots 12, 13 and 14. Three copies of
one function, taking an **increment** and a **target angle**:

```
target = wrap(target) into [-pi, +pi]      ONE step, not a modulo
if |target| != pi:
    changed = |current - target| > pi/180
else:
    target  = current >= 0 ? +pi : -pi     fsel
    changed = | |current| - pi | > pi/180
if changed:
    [target] = target ; [flag] = 1 (a byte) ; [accumulator] += increment
return changed ? 0 : 1
```

**The tolerance is one degree** — `0x82675554` is π/180, the same word slot 32
uses to convert its angles. A command within a degree of where the model already
points is **discarded, increment and all**. A port that always stored would
accumulate a command retail drops.

**The wrap is one step.** Nothing loops, so a target three turns out stays three
turns out. Reproduced rather than repaired.

## And a field mapping met coming the other way

The accumulators are **not in slot order**: slot 12 → `+36`, slot 13 → `+44`,
slot 14 → `+40`.

Cycle 1388 derived exactly that crossing from the **consumer's** side — `+36`
drives `at304`, `+44` drives `at308`, `+40` drives `at312` — by reading which
command each axis block loads. The two derivations share no code and no
reasoning, and they agree.

That is the second time in this campaign a mapping has been met from both ends;
the first was `float_slot_for_bit` at cycle 1353. It is the strongest form of
confirmation available without an oracle.

## The differential

```
flight_command_microexec=pass cases=30 values_compared=120
```

Thirty cases — all three slots — and **four things compared each**: the return
code in `r3`, the accumulator, the target field, and the **flag byte**. A port
that stored the target without the flag, or the flag without the increment,
fails here and nowhere else. Passing on the first run.

The `pi-negzero` case is the one worth naming: `fsel` compares against **+0.0**,
so a current angle of −0.0 takes **+π**. Cycle 1323 measured that an idle axis
leaves negative zero in the input record, and cycle 1354 carried the same rule
into the binding layer — so this is the third place in the chain where that one
detail decides a branch.

## What a demo now needs

The model's side is closed. Missing, in order of cost:

1. **the wiring** — nothing calls these setters from the contracted input path;
2. **a camera** (A3.3, unstarted, converging on the transform kernel already
   contracted);
3. **something to draw**, which is JV and remains an open architectural decision.

A proof-of-concept flying a synthetic scene needs 1 and 2 only. That is the
cheaper demo and it is honest as long as its caption says what it is.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 15 (1354–1356, 1372, 1373, 1375, 1380, 1381, 1387–1393) |

Eight consecutive cycles ending with a contracted behaviour.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 44
tools/tests                           Ran 77 tests, OK
flight_command_microexec              pass 30 cases, 120 values
```

## Next

**The wiring.** A native harness that drives the contracted chain end to end from
a recorded input log: `RetailInputLog` → the contracted binding layer → these
three setters → the step → the export block, one frame at a time, with the
position and orientation written out per frame. No renderer, no camera — a
column of numbers that is the aeroplane's trajectory. That is the smallest thing
that is unmistakably *flying*, it needs no decision anyone has deferred, and it
is the substrate the demo draws.
