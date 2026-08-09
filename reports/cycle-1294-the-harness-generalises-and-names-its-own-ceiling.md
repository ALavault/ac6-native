# Cycle 1294 — the harness generalises, and names its own ceiling

## Qualification

- Ghidra projects `ghidra-projects/ace-combat-6` **and**
  `ghidra-projects-xenon/ac6-xenon`; which one is used is a finding of this
  cycle rather than a detail of it.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness itself.
- Payload: Mission 01 scenario root node,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **No oracle pass was spent.** No console emulator, no bridge, no native run.
  No product C++ changed.

## Why

The session's plan makes differential p-code micro-execution the acceptance
standard for gameplay: a flight, weapon or damage behaviour is *1:1* only if the
native function and the retail function write the same thing on the same
synthetic state. Cycle 1089 built that instrument for one shape — a parser, a
payload in, a record and a buffer out, at three hard-coded bases and three
integer arguments. A gameplay function has another shape, so the harness is
parameterised rather than copied.

## What was built

`scripts/MicroExecuteFunction.java`. Named regions instead of three fixed bases;
integer **and float** arguments; parameterised call interception instead of only
the error printer `0x823828B8`; register capture; and a `--batch` mode, because
a matrix of cases is the normal shape of a gameplay question and paying Ghidra's
startup once per case instead of once per matrix decides whether the instrument
is usable.

The spec is line-oriented and committed beside its output, so a case is
reproducible from the repository rather than from a shell history.

## Measuring it before using it

`CLAUDE.md` requires the instrument be measured first, and cycle 1089's own
lesson is that four passing cases were an anecdote — widening to 24 nodes made 9
of them diverge.

`tools/audit_microexec_harness_calibration.py` regenerates **every one of the
138 committed cases** through the new harness and compares the semantic payload,
which is exactly what `tools/compare_ac6_function_snapshots.py` compares:
`exit`, `registers`, `calls`, `memory_writes`. Provenance is excluded and does
differ — the general harness describes named regions where the old one named a
payload, a record and a buffer.

| control | result |
| --- | --- |
| semantic payloads identical | **138 / 138** |
| digests identical to `reader-digests.tsv` | **138 / 138** |

The digest control is the one that matters downstream: it is the artefact the
native `*Bin` readers are actually tested against.

One deliberate non-reuse: `compare_ac6_function_snapshots.py` **refuses** two
snapshots that both declare `ppc-pcode`, which is correct — it exists to compare
a retail run against a native one. Calibration is a different question and got
its own tool rather than a falsified `implementation` field.

## The first non-parser case, and the ceiling it found

`0x822A1E80` — 40 instructions by `.pdata`, three float arguments, computes
`r31 = r3 + 0x80`. The right shape to try first.

**It does not run in the canonical project.** `InstructionDecodeException` at
`0x822A1EC0`, 16 steps in. `ghidra-projects/ace-combat-6`'s PPC module does not
decode VMX128: `DumpRange` prints `<not-disassembled>` at seven of the forty
slots, all primary opcode 4.

**It runs in `ghidra-projects-xenon/ac6-xenon`**, which decodes them
(`lvx128`, `stvx128`) *and* whose emulator executes them — 142 steps, two callee
entries, 48 bytes written. Cycle 1127 read `stvx128` in that project and said so
in its qualification; nothing before now depended on the difference, because the
`*Bin` readers contain no vector instruction at all.

**The residual limit is not "VMX128". It is a finite, named list.** The run
stops on `UnimplementedCallOtherException (loadVectorLeftIndexed128)` — a
decoded instruction with no emulation behaviour. `scripts/Ac6CallOtherCensus.java`
counts them over `0x82000000..0x82400000`:

**70 distinct p-code operations, 15,945 sites, 868,523 instructions.**

| op | sites | first |
| --- | ---: | --- |
| `vectorMergeHighWord` (`vmrghw`) | 4,228 | `0x820998E8` |
| `vectorMergeLowWord` (`vmrglw`) | 1,542 | `0x820998F4` |
| `loadVectorLeftIndexed128` (`lvlx`) | 1,167 | `0x820A1450` |
| `vectorMultiplyAddFloatingPoint` (`vmaddfp`) | 1,096 | `0x8209CA6C` |
| `trapWord` (`twi`) | 824 | `0x820B77D4` |

`EmulatorHelper.registerCallOtherCallback(String, BreakCallBack)` exists —
checked with `javap` against the shipped jars, not assumed from the class name.
So the list is suppliable.

**And that is a weaker kind of evidence, which has to be labelled.** A
`vmaddfp` behaviour written here is *my model of vmaddfp*, not retail code being
executed. It is architecture semantics rather than game semantics, and it is
testable against published vectors — but a snapshot produced with hand-written
vector behaviours is not the same claim as one produced without, and the harness
will have to say which it is.

## Two corrections

**`exports/` under-reports callers, not only instructions.** `CLAUDE.md` already
warns that `exports/` truncates VMX128-heavy functions —
`exports/822a1e80.json` carries 16 of the 40 instructions. Its `callers` field
is also short: it lists **one**, `0x822955F0`. A full scan finds **four** —
`822957BC`, `82295E60`, `822A27F4`, `8230B44C`. I nearly recorded the ladder's
attribution of this function to `0x822A23D8` as a contradiction on the strength
of that field. `822A27F4` is inside `0x822A23D8`, and the ladder is right. The
truncation warning should be read as covering the whole record, not the
disassembly.

**A partial run is not a result.** The 48 bytes the faulting run wrote land at
`object+0x90` and read as three 16-byte rows of an identity matrix
(`3f800000` at word 0, word 5 and word 10). That is consistent with the ladder's
`+0x90/+0xA0/+0xB0`, and it establishes the **initialisation**, not the
rotation — the function stopped before the callee that applies the angles.
`registers.f1` on return is `0x3FE0…` = 0.5, which is argument `f2` after
`fmr f1,f2`: scratch from a function that never returned. Neither is evidence
about what `0x822A1E80` computes.

## Not established

- Anything about what `0x822A1E80` writes when it completes.
- Whether the 70 operations are all reachable from gameplay code, or whether the
  subset a flight update needs is small. The census is over the whole image; it
  sizes the gap and does not partition it.
- Whether any of the 70 already has an emulation behaviour in this Ghidra build.
  One was tried and refused; sixty-nine were not.
- Whether the two projects agree on anything else. They are separately analysed
  databases over the same bytes, and only the disassembly of one range was
  compared here.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27 (1 skipped, no display)
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: 47 tests, OK
```

`audit_ac6_contract_artifacts.py` fails on `mission01-native-gate.json` alone,
which commit `f4bdb328` bannered as superseded **with the reason it cannot
pass**. It is untouched by this cycle.

## Next

The plan's Phase 0 is larger than it was written: it now includes a vector
behaviour library, and the census is its scope. Before writing one, partition
the census — which operations does the *player update path* actually reach — so
the library is sized by the question rather than by the image.
