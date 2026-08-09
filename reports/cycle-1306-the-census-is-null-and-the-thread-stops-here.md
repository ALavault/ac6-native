# Cycle 1306 — the census is null, and the thread stops here

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The A/B, run properly

Cycle 1305 inferred that neither reading of `vpermwi128` rescues `0x822A1E80`
from two runs made for other purposes. This runs it as the experiment it should
have been: the second reading implemented as its own override
(`vpermwi128-lowfirst`), applied uniformly at **all six** sites, at zero angles
where the target is the identity.

| reading | `+0x90` | `+0xA0` | `+0xB0` |
|---|---|---|---|
| high-first (Xenia's code) | `1 1 0 0` | `0 1 0 0` | `0 0 0 0` |
| low-first (Xenia's comment, and the module) | `0 0 0 0` | `0 0 0 0` | `0 0 0 0` |
| **target** | `1 0 0 0` | `0 1 0 0` | `0 0 1 0` |

Neither is the identity. The routine does not adjudicate, now measured rather
than inferred.

## The census is null

If the compiler ever emitted `vpermwi128` as a move, the identity immediate would
name the convention: `0xE4` is the identity under low-first, `0x1B` under
high-first.

Over **545 sites in the image, neither value occurs.** Not once. The most common
are `0xA0` (55), `0x8D` (38), `0x80`, `0x6D`, `0x60` (32 each). No identity
swizzle is emitted — reasonably, since `vor` is the cheaper move — so the
population cannot settle the convention.

That is a clean negative and it closes the last cheap avenue. Fitting a
convention to the distribution of the other 71 immediates would be the numerology
cycle 1300 refused, and it is refused here too.

## The decision

**This thread stops.** The question needs an execution oracle — something that
runs the instruction and says what it produced — and the plan this session began
with put oracles out of scope by design.

What is left behind is not nothing:

- a calibrated harness, **138/138** against the committed corpus;
- a vector-layer suite, **23 cases over 12 mnemonics**, all measured;
- four SLEIGH defects found, three of them only by using the instrument, two
  confirmed, one disputed and pinned as disputed;
- `0x8209CB70` reproduced exactly — `sin` and `cos` to the bit at seven angles
  including the argument reduction;
- `MISSION01_LADDER.md` now carries a section stating what the instrument does,
  what it cannot certify, and why.

**And the cost should be stated as plainly as the result.** Thirteen cycles, no
gameplay behaviour derived, no contract entry added. The bet was that the
micro-execution harness is the backbone of "1:1", and the harness turned out to
need four repairs before it could be trusted for vector code. That was worth
finding — a suite of sixteen green tests that could not see a severed register
file is exactly the failure this campaign exists to catch — but it is thirteen
cycles of instrument work charged to a gameplay plan, and calling it progress
toward JP would be false.

## What the instrument is good for now

Scalar and integer routines, and vector routines that stay inside one register
family. `0x8209CB70` is the proof. What it cannot certify is any routine whose
result flows through `vpermwi128` — which includes `0x822A1E80` and its two
siblings.

The plan's Thread A should therefore start somewhere that does not need it. The
input path (A4) and the objective/debrief logic (A6) are both scalar; the flight
model (A3) is the one that will hit vector code first, and it now has a known
obstacle waiting for it rather than an unknown one.

## Not established

- Which reading of `vpermwi128` is the hardware's.
- What `0x822A1E80` computes. Fourteen cycles; no claim, and none is owed.
- Whether a defect beyond the four found remains on that routine's path. At
  least one does, since neither reading yields the identity.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
vmx128_behaviours=pass (23/23, 2 confirmed defects, 1 disagreement)
instrument_discipline_index=pass shapes=18 unindexed=0
```

## Next

Thread A, starting on scalar ground: re-derive the input path statically, since
the "canonical" one in `CURRENT_PLAN.md` is superseded and came from a bridge
that is no longer in the workspace. `src/sdl_input.cpp` already reads the device;
what is missing is the retail mapping from axis to flight quantity, and that
mapping is integer and scalar all the way to the point where it meets the flight
maths.
