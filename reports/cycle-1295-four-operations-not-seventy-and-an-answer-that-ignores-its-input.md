# Cycle 1295 — four operations, not seventy, and an answer that ignores its input

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon` for everything vector;
  `ghidra-projects/ace-combat-6` for the calibration re-run.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** Xenia's source and the Cell BE SIMD PEM were
  read **as documentation of what an instruction means**. No game code ran
  anywhere, no game behaviour was observed, and nothing was compared against a
  running emulator. That is a different act from an oracle pass and this cycle
  does not consume one.
- No product C++ changed.

## The scope, measured instead of assumed

Cycle 1294 counted **70** unimplemented p-code operations over the whole image
and left the plan sized by that number. It is the wrong number for any actual
question.

`0x822A1E80`'s closure — itself plus `0x820A9B30`, `0x820A99F8`, `0x82211828`,
**271 instructions** by `.pdata` — needs **four**:

| operation | sites in the closure |
| --- | ---: |
| `vectorMergeHighWord` (`vmrghw`) | 39 |
| `vectorMergeLowWord` (`vmrglw`) | 12 |
| `loadVectorLeftIndexed128` (`lvlx`) | 12 |
| `vectorRotateLeftImmediateMaskInsert128` (`vrlimi128`) | 9 |

`0x822A1E80` itself emits **zero**: `lvx128` and `stvx128` carry real p-code,
which is why cycle 1294's run got 16 steps in before dying and not 0.

## Operands read, not looked up

A CALLOTHER behaviour receives whatever the SLEIGH module chose to pass, and the
module is free to choose. `scripts/Ac6PcodeDump.java` reads it out of the build
in use:

```
vs45:16 = CALLOTHER<loadVectorLeftIndexed128>(r0:8, r8:8)
vr12:16 = CALLOTHER<vectorRotateLeftImmediateMaskInsert128>(vr12:16, vr13:16, 0x4:1, 0x3:1)
vs38:16 = CALLOTHER<vectorMergeHighWord>(vs42:16, vs40:16)
```

Two things that a reading of the documentation would have got wrong:

- **`vrlimi128` takes its own destination as an input**, because it inserts
  under a mask and has to preserve the elements the mask does not select.
- **`lvlx` is passed `rA` verbatim**, register `r0` included. The `(rA|0)` rule
  — in an indexed form `rA = r0` means the literal zero — is therefore the
  harness's job, and it is decidable from the varnode's own identity.

The third and fourth operands of `vrlimi128` were **not** trusted to the
disassembler's print order. Decoding the instruction word settles it
independently:

```
0x820A9B98 = 0x19846FD0
  VD128=12  VB128=13  IMM=0x4  z=3
```

so operand 3 is the mask and operand 4 the rotate, which is what the mapping
assumed. Cross-references for the semantics: the Cell BE SIMD PEM v2.07c, the
only place the `lvlx`/`lvrx` family is documented, and Xenia's
`ppc_emit_altivec.cc` for `vrlimi128`'s mask-bit order.

## The harness declares when it is guessing

`vmx on` in a spec registers the four behaviours. A snapshot produced that way
carries, in provenance:

```json
"asserted_semantics_enabled": true,
"asserted_semantics": {
  "loadVectorLeftIndexed128": 12,
  "vectorRotateLeftImmediateMaskInsert128": 9,
  "vectorMergeHighWord": 39,
  "vectorMergeLowWord": 12
}
```

Everything else this harness produces is retail instructions executing. This is
**my model of an instruction standing in for one**, and the difference has to
survive being read quickly, so it is a field rather than a footnote. Off by
default: a snapshot without the flag leaned on nothing.

The firing counts equal the static census exactly — 72 sites, 72 firings — so
the closure is straight-line and every site ran once.

## The function completes. The answer is still worthless.

`0x822A1E80` goes from `fault` at 16 steps to **`exit: return` at 547 steps**,
six callee entries, 48 bytes written at `object+0x90`.

**And then the control killed it.** Three runs at three angle triples —
`(0,0,0)`, `(0.25,0.5,0.75)`, `(π/2,0,0)` — write **byte-identical** output:

```
+0x90  00000000 00000000 3f800000 00000000
+0xA0  00000000 00000000 00000000 00000000
+0xB0  00000000 00000000 00000000 00000000
```

A rotation matrix at zero angles is the identity, and this is not it at any
angle. An output that does not move when the input does is a dead channel, and
the three runs cost one batch invocation.

The floats do reach the function: captured `f1` on return is `0.0`, `0.75`,
`0.0` across the three — that is argument `f3` after `fmr f1,f30`, moving as the
listing says. So the arguments arrive and the vector path loses them.

**So no claim is made about what `0x822A1E80` computes.** At least one of the
four behaviours is wrong, or the callees need state the spec does not set up.
Cycle 1294's partial-run identity matrix at `+0x90/A0/B0` also stops being
usable evidence: this cycle overwrote it with the completed run's zeros, and
neither is corroborated.

## What I did wrong, and it is a shape

I wrote four instruments and validated **none of them individually**, then
debugged them through a 547-step composite of 271 instructions. The four could
have been checked in isolation, in minutes, against hand-computed vectors —
`vmrghw` of two known registers has one right answer.

This is the discipline's own rule applied to a plural: *measure the instrument
before trusting it* was followed for the harness in cycle 1294, with 138 cases,
and skipped for the four behaviours the harness now depends on. A composite of
unvalidated parts localises nothing when it fails.

The angle control is the only reason this is a negative rather than a published
rotation matrix.

## Calibration re-run

The harness was edited after cycle 1294 measured it, so it was measured again:
**138/138 semantic payloads identical, 138/138 digests identical.** The new
provenance fields sit where the comparison ignores them, by construction.

## Not established

- What `0x822A1E80` writes.
- Which of the four behaviours is wrong, or whether the fault is the setup
  rather than a behaviour.
- Whether `defaultSpace()` is the space the module's `LOAD` uses. The p-code
  names space `0x1a1`; the harness assumes the program's default. Untested, and
  it would produce exactly this symptom.
- Anything about the other 66 operations.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: 47 tests, OK
microexec_harness_calibration=pass (138/138)
```

## Next

Validate the four behaviours one at a time, each against a hand-computed vector,
before any of them is used in a composite again. `lvlx` first, and the address
space with it, because a load that silently returns zeros explains every symptom
above by itself.
