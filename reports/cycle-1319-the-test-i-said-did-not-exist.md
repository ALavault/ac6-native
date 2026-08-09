# Cycle 1319 — the test I said did not exist

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The correction, first

Cycles 1309 and 1314 both stated that Xenia has **no conformance test** for
`vpermwi128`, on the strength of

```
grep -rln vpermwi src/xenia/cpu/testing/     -> nothing
```

**The file exists.** It is at `src/xenia/cpu/ppc/testing/instr_vpermwi128.s` —
one directory deeper than I searched, alongside **167 other per-instruction
tests**. My grep's population was the wrong population, and I published a
negative from it twice.

That is the *seventeenth shape* — the right search, run against a sibling — and
it stood for five cycles because nothing downstream contradicted it. The
technique this file's own discipline prescribes would have caught it in one
command: run the search against a case whose answer you already know is not zero.
`instr_add.s` would have done it.

The four vectors, with source `[00010203, 04050607, 08090A0B, 0C0D0E0F]`:

| immediate | result | meaning |
|---|---|---|
| `0x1B` | `[x, y, z, w]` | identity |
| `0xE4` | `[w, z, y, x]` | reverse |
| `0x00` | `[x, x, x, x]` | broadcast of x |
| `0xFF` | `[w, w, w, w]` | broadcast of w |

**They confirm high-first independently.** `0x1B = 0b00 01 10 11`: the high pair
is `0`, so element 0 takes source 0 — the identity. Under low-first, `0x1B` would
put `0x1B & 3 = 3` in element 0 and produce the reverse.

So the adjudication now rests on **three** sources — Xenia's emitter, Xenia's
conformance test, and the 545-site cross-match — and it is Xenia's *comment*
alone that is wrong. `tools/audit_vpermwi128_crossmatch.py` now checks all four
vectors before it scores anything, in Python rather than through the harness,
because the image contains neither `0x1B` nor `0xE4` and so cannot run them.

## Two more corrections adopted

**Below-deadzone slots are zero, not stale.** Cycle 1315 wrote that a skipped
lane "keeps whatever the caller left there". That is wrong on this path: the
slot is inside the `0x84` the frame stage clears every frame, so a lane below the
deadzone reads as **zero**. The artefact says so now.

**"Materialised nowhere" was over-claimed.** Cycle 1315 said `0x826EDBA0` "is
materialised nowhere, so no function reaches a record by its own address". The
first half is a measurement; the second is an inference a materialisation scan
cannot support — a record can be reached from the base or from a pointer passed
in. The artefact now says *no direct absolute materialisation was observed*.

## The consumer differential, and where it stops

The harness gained a `hint` directive: a p-code operation the architecture
defines as having **no effect on program state** becomes a no-op, counted in a
`hint_noops` field separate from `asserted_semantics`. A supplied nothing is not
a supplied model and the snapshot must not conflate them. `dataCacheBlockTouch`
and `dataCacheBlockTouchForStore` are the first two.

`tools/audit_input_consumer_microexec.py` builds the whole service in memory —
the singleton at `0x8290DE00`, its `DriverContext`, five driver pointers, four
`DriverController`s each carrying a different axis at a different value so the
output would name the index-to-record mapping.

It reaches **655 steps and stops**, in the vectorised `memcpy` the accessor
tail-calls, on `loadVectorForShiftLeft`. The two operands are read rather than
guessed:

```
821f3978  lvsl  v0,r0,r31        vs32:16 = CALLOTHER<loadVectorForShiftLeft>(r0:8, r31:8)
821f399c  vperm v13,v13,v12,v0   vs45:16 = CALLOTHER<vectorPermute>(vs45:16, vs44:16, vs32:16)
```

Two behaviours away, and both are standard. **The differential is not finished
and this report does not claim it is.**

## Not established

- The consumer differential.
- `r20` derived from all callers rather than from within the function — cycle
  1318 exhausted the function, which does not cover a caller that sets it before
  the `bl`. Both callers are known: `0x821CA908` and `0x821CB5F0`.

## A scope correction to carry

The eleven mnemonics cycle 1317 called tested are qualified **for this closure**,
at the sites and immediates that occur in it — not globally exact. `vmrghw` is
right at two sites and two aliasings, which is not the same as right.

## Gates

```
mission01_final_gate (playable-v1)  JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28
vpermwi128_crossmatch=pass  4/4 conformance, 545/545 cross-match
tools/tests: Ran 72 tests, OK
```

## Next

`lvsl` and `vperm`, whose operands are now read, then the consumer differential.
Then `r20` from both callers, and an A/B with four distinct sentinels — which is
what the built service was already designed to answer.
