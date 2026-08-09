# Cycle 1382 — the bridge was not the blocker

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays 37. **No contract entry** — this cycle is a
  measurement and a correction.
- New artefact `analysis/microexec/calibration/live-rotation-blockers.tsv`.

## The prediction, and it was wrong

Cycle 1381 ended: *"Running the composite with slot 31 **live** needs the VMX128
register-file bridge that cycle 1301 built and that has not been used since."*

The run was made twice, identical except for `alias on`:

| | steps | callees entered | exit |
|---|---:|---:|---|
| without the bridge | 601 | 18 | fault |
| **with the bridge** | **601** | **18** | **the same fault** |

Same step count, same instruction, same message. **The bridge was never
exercised and is not the blocker.**

Stating that plainly matters more than the fact itself: the prediction was made
in the confident register of a "Next" section, and a cycle that started by
building the bridge harness would have spent itself before discovering the
failure was somewhere else. Running the cheap version first — the same case with
and without — cost one Ghidra pass and settled it.

## What actually stops it

`UnimplementedCallOtherException: loadVectorLeftIndexed128, PC=820A9A64`.

`0x820A9A64` is inside **`sub_820A99F8`** — `rotate_820A99F8` in
`retail_transform.cpp`, already ported and contracted under A3.1.

And it was reached with slot 32 **stubbed**, which is its own finding: the
**integrator calls the rotation directly**. `0x82303110 → 0x820A99F8` is one hop.
The rotation is not the orientation update's private helper.

## The cost, bounded exactly

The integrator's own fifteen vector mnemonics all execute — 601 steps got that
far. A rotation needs nine, of which **four** are in the census of operations the
SLEIGH module emits without a body:

| mnemonic | p-code op | sites | kind |
|---|---|---:|---|
| `vmrghw` | `vectorMergeHighWord` | 4228 | permute |
| `vmrglw` | `vectorMergeLowWord` | 1542 | permute |
| `lvlx` | `loadVectorLeftIndexed128` | 1167 | load |
| `vrlimi128` | `vectorRotateLeftImmediateMaskInsert128` | 734 | permute |

**7,671 of the 15,945 CALLOTHER sites in the image — 48%.**

Four operations, and all four are **pure data movement**. None performs
arithmetic, so none can fail in a way that produces a plausible number: an error
puts bytes in the wrong lane, not a value one ulp out. That is the cheapest kind
of asserted semantics to control, which is the argument for supplying them rather
than routing around them.

## The status they would carry, and the control that already exists

Anything supplied through `registerCallOtherCallback` is **asserted ISA
semantics, not executed retail code**, and the harness already labels it — every
snapshot carries `asserted_semantics`. The `vpermwi128` override is the
precedent, and the standard it set is that a model is generated from a census of
its own sites and validated against every immediate the image actually uses.

The control here is free. `rotate_820A9B30`, `rotate_820A99F8` and
`rotate_82211828` are already ported and contracted under A3.1, and cycle 1381's
composite already knows the expected position block from the integrator's own
contracted differential. **Four models can be checked against results that were
established without them** — which is the only way asserted semantics can be
trusted at all.

## Not established

- Whether four is the whole list. It is the list for `sub_820A99F8`; a live run
  will find the next one if there is one, and that is the honest way to discover
  it rather than reading nine mnemonics and hoping.
- The composed effect on the position block, still.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 28 (1351–1371, 1374, 1376–1379, 1382) |
| implementation/integration spent on A3.2 | 8 (1354–1356, 1372, 1373, 1375, 1380, 1381) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 18 behaviours
ctest                                 100% passed, 0 failed out of 37
tools/tests                           Ran 77 tests, OK
```

## Next

Supply the four, one at a time, each generated from its own census and each
validated against a case whose answer is already contracted — `vmrghw` and
`vmrglw` first, because they are the two largest and the two simplest, and
because a wrong lane in a merge is visible in a single sixteen-byte dump. Then
re-run cycle 1381's composite with slot 31 live and see whether the position
block agrees with the integrator's own contracted differential. If it does, the
four models are corroborated by a result derived without them; if it does not,
the disagreement is in the models and not in the port.
