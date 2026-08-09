# Cycle 1320 — the behaviour that could not fire

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No console emulator, no bridge, no game run.
- No product C++ changed.

## The consumer differential finishes

`0x821CAA50` runs to **`return`**, 4740 steps, 144 callee entries, 34 stubbed
calls, **168 bytes written into the record region**. Cycle 1319 stopped it at 655
steps; what it needed was one asserted behaviour and the register-file bridge.

`lvsl` was the behaviour. The bridge is the other half, and the memcpy at
`0x821F398C` shows why in four instructions:

```
821f398c  lvx128 vr13,r0,r31        writes the VMX128 file
821f399c  vperm  v13,v13,v12,v0     reads and writes the AltiVec file
821f39a4  stvx128 vr13,r0,r30       reads the VMX128 file
```

`v13` is `vs45` and `vr13` is a **different register** — `register:0x42d0` against
`register:0x100d0`, neither a parent of the other. That is cycle 1301's
measurement re-taken from the language itself rather than from behaviour. The
loop crosses the boundary twice per iteration, so without the bridge it copies
nothing and with it copies 104 registers' worth.

## The correction: a model I wrote that can never run

`vperm` emits `CALLOTHER<vectorPermute>` and sits in the CALLOTHER census beside
the four asserted behaviours, so this cycle implemented it. **It never fired.**

Ghidra's PowerPC module registers a `vectorPermuteOpBehavior` from
`ghidra.program.emulation.PPCEmulateInstructionStateModifier` — a layer below
SLEIGH — and that registration wins over `registerCallOtherCallback`. Measured,
not deduced: running `0x821F399C` with `vmx` **off**, no callback of ours
registered at all, still returns the correct ISA permute.

The model was deleted rather than left in place. An asserted behaviour that
cannot fire is worse than no behaviour: the snapshot would carry
`asserted_semantics_enabled` while the value came from somewhere else, and a
reader would credit the wrong source.

**And the census header was wrong.** It said *"a name here is an instruction the
emulator will refuse unless the harness registers a behaviour for it"*. That is
false for at least one of its 70 rows, `vectorPermute`, 161 sites. The file
censuses the **language**; which rows the emulator already covers is a separate
question and only running one answers it. Header corrected.

## The fixture that could not fail

The first `vperm` fixture was the 32-byte suite fixture split in half — every
byte distinct, byte *k* equal to *k*. Under it the expected output is
`out[i] = cat[control[i]] = control[i]`: **the control vector itself**. Both
cases matched on the first run while nothing fired, because something was putting
the control vector in the destination and a fixture whose right answer is one of
its own inputs cannot tell that apart from a permute.

Only the `matched but no behaviour fired` guard saw it. The concatenation is now
scrambled, `byte k = (7k + 0x13) & 0xFF`, equal to `k` for no `k`.

That is the **twenty-eighth shape**, and it is not the twenty-seventh wearing a
new face: that one was about fixtures inheriting the subject's *convention*. This
one is a fixture whose expected output is a **copy of one of its inputs**, so the
null hypothesis "the instruction moved an operand" survives the test.

## The bridge had no control, and it does now

Cycle 1317 named "the harness setup" as one of two remaining explanations for
`0x822A1E80`. The bridge **is** harness setup — and **no committed tool had ever
emitted `alias on`**. It was used in composites and never isolated, under an open
hunt that named it.

Two cases now, one per direction, each the counterpart of a pinned defect:
`bridge-vr-to-vs` (`lvx128 vr13` seen through `vs45`) and `bridge-vs-to-vr`
(`vspltw v5` seen through `vr5`, the same site as the pinned
`register-file-alias` defect, with the only difference being `alias on`). Both
pass. The suite is **32/32** over 12 mnemonics, with four pinned module defects.

## What the records say

Three controller plans, because one cannot answer the question — one axis per
controller, one value per controller, and one controller per driver-pointer slot.
The third exists because in the first two "controller 0" and "pointer slot 0" are
the same statement.

**All three give byte-identical records at an identical 4740 steps.** Every one
of the four records is filled from **driver-pointer slot 0**; slots 1 to 4 are
not read on this path.

| offset | measured |
|---|---|
| `+0x04` | four bytes, zero |
| `+0x0B` | the run sets **bit 5** and no other |
| `+0x4C` | LX = `-0.0` |
| `+0x50` | LY = `float32(30000/32767)` = `0x3F6A61D5` |
| `+0x54` | RX = `-0.0` |
| `+0x58` | RY = `-0.0` |
| `+0x8C…+0x94` | nine bytes, zero |
| `+0x98` | the constant `3` |

`+0x0B` is **not** the value `0xED` the raw run reports. Poison A is `0xCD` and
`0xED = 0xCD | 0x20`: it is a read-modify-write, and reporting `0xED` would
publish poison as data. The checker now reports the bits set, not the byte.

**An untouched axis reads negative zero.** A native port writing `+0.0` there
agrees numerically and differs byte for byte.

## Two corrections to predecessors, and one to myself

**Cycle 1316 put record 0 eight bytes too high**, at `0x826EDBA0`, and every
cycle since repeated it. The base is `0x826EDB98` — the array base itself.

The run addresses alone do **not** establish that: the write runs begin at
`0x826EDB9C`, four bytes above the base, because `+0x00..+0x03` is not written on
this path, and that is equally consistent with a base of `0x826EDB9C`. What
settles it is a derivation that never met this one — cycle 1318 read LY into slot
`+0x50` from the mask bits alone, and only `0x826EDB98` puts
`float32(30000/32767)` there. `0x826EDB9C` would put it in LX's slot.

**Cycle 1319's own docstring**, mine, said "the index the API receives is a
constant 1 while the loop runs 0..3". That misread which argument was which.
`0x821CAA9C` calls `0x82337E70` with `(1, r20)`: the **1 is the first argument**
and the index is `r20`.

## `r20`, from the callers this time

Cycle 1318 exhausted `r20` inside the function — a search whose population
excludes a caller that sets it before the `bl`, which is the seventeenth shape
and the reason correction #6 asked for this. All three callers checked with a new
`scripts/Ac6RegisterWriters.java`:

| function | instructions | writes `r20` | reads `r20` |
|---|---|---|---|
| `0x821CA908` | 82 | 0 | 0 |
| `0x821CB5F0` | 73 | 0 | 0 |
| `0x821D7A90` | 124 | 0 | 0 |

None of them touches it. Combined with the single `li r20,0x0` at `0x821CAA88`,
which dominates both reads, `r20 = 0` is settled from outside as well as inside —
and the A/B confirms it by execution: slot 0, three plans, no variation.

## Not established

- What bit 5 of the flag word means, and what the constant `3` at `+0x98` is.
- Whether a *retail* service ever presents more than one connected controller on
  this path. What is measured is that **this** code reads slot 0 only; the
  built service is synthetic and cannot speak for the retail one's contents.
- `0x822A1E80`. The bridge now has controls and passes them, which removes it
  from cycle 1317's short list of two and leaves the expectation and the rest of
  the setup.
- The consumer is executed, not ported. No product C++ and no contract entry.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none
ctest                                100% passed, 0 failed out of 28
vmx128_behaviours                    pass, 32/32, 4 pinned module defects
contract_addresses                   pass, 144 cited, 144 supported
contract_derivations                 pass, 27 behaviours, 0 gaps
tools/tests                          Ran 72 tests, OK
instrument_discipline_index          pass, 19 shapes, 0 unindexed
contract_artifacts (live contracts)  pass, 50 cited, 50 match HEAD
```

`refresh_contract_evidence.py` was run and **correctly refused**: none of the
three edited artefacts is cited by any contract, so there is nothing to re-pin.

`audit_ac6_contract_artifacts.py` over `analysis/contracts/*.json` fails on seven
paths, all of them in `mission01-native-gate.json`, which carries a
`superseded_by` banner and whose artefacts were never in the tree. **This
predates the cycle** — it fails identically with these changes stashed. It is
recorded rather than worked around; whether the auditor should skip a contract
that declares itself superseded is a decision, not a side effect.

## Next

The consumer is executed but not ported. The port is the next behaviour for
`mission01-playable-gate-v1.json`, and it now has a differential to be checked
against rather than a reading. Then `0x82211DF8` and the float it receives —
which the ladder does not name and this cycle will not name in advance.
