# Cycle 1324 — the control that failed correctly

## Qualification

- No Ghidra run and **no oracle pass**. This cycle executed no retail code; it
  consumed the 321 vectors cycle 1323 committed.
- **Product C++ changed**: `RetailInputLog`, `replay_input_log`, and their tests.

## A7 criterion 7 is met

Deterministic input replay, on the retail path.

`ac6::ReplayLog` already existed and is **not** this. It logs the product's own
`InputFrame` — pitch, roll, yaw, throttle, buttons — which is an abstraction the
product chose. `RetailInputLog` logs the **0x40 bytes `0x8234D0A0` copies**,
because that is the only point on this path where a capture is a capture and not
an interpretation. Reusing the existing log would have recorded a decoded view
and called it retail input.

The file carries the guest bytes verbatim. They are already big-endian and are
not reinterpreted, so a log written on one host reads identically on another with
no byte-order clause — which is a consequence of logging the snapshot rather than
a structure, not a separate design decision.

Four assertions, over the same 321 snapshots the differential uses:

- replaying one log twice gives identical records **and** identical digest;
- every replayed record equals what `build_input_record` produces directly, so
  the two paths cannot drift apart;
- a file round trip changes neither;
- a truncated trailing frame is **rejected**, because accepting it would replay a
  record built from bytes that were never captured.

Digest `0x4B08109C2C631941`, FNV-1a 64 over every encoded record in order — the
same construction `tools/emit_ac6_reader_digests.py` uses on micro-execution
writes, so a divergence anywhere in 321 frames surfaces as one number.

## The control failed, and it was right to

The negative control flips one bit in one frame of 321 and requires the digest to
move. The first version flipped the **top** byte of the held word — and the test
went red.

That is the correct answer. Device bits 16..31 are measured unmapped (cycle
1321), so flipping bit 31 changes no record byte and **must not** change the
digest. My control was testing that the digest is sensitive to everything, which
is not the property wanted and is not true.

It is now a **pair**:

```
flipping device bit 0,  which is mapped,   moves the digest
flipping device bit 31, which is unmapped, does not
```

One of those alone is a weak test. Together they say the digest is sensitive to
exactly the input the port reads, and the second one re-derives cycle 1321's
unmapped-bit finding from the product side rather than restating it.

This is the discipline's own shape — *measure the instrument before trusting it*
— arriving one level up: the instrument here was the control, and it was wrong
before the thing it guards was.

## Not established

- Replay against a **live** capture. The 321 snapshots are synthetic sweep
  states, not a session; nothing has yet recorded a human flying anything. The
  mechanism is proven, the corpus is not a game.
- Everything cycle 1323 left open is still open: the flag threshold's form, what
  `device+0x38`/`+0x3A` are, the constant 3 at `record+0x98`.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 10 behaviours
ctest                                100% passed, 0 failed out of 29
contract_addresses                   pass, 155 cited, 155 supported
contract_derivations                 pass, 28 behaviours, 0 gaps
tools/tests                          Ran 72 tests, OK
refresh_contract_evidence            pass, 2 paths, 0 uncited, 2 changed
```

## The two estimates

| kind | cycles | delta |
|---|---:|---|
| shared instrument (not an A7 cost) | 17 | — |
| A7 research | 12 | — |
| A7 implementation / integration | 3 | +1 |

**A7 is closed.** Seven criteria, seven met, with criterion 4 sampled rather than
exhaustive and said so in both the report and the tool.

## Next

A3.1: one shared `RetailTransformKernel`. The five sentinel capsules, the same
capsule through Ghidra micro-execution and the XenonRecomp generated C++ for this
XEX, and a comparison of **architectural facts** — basic blocks, load and store
addresses and sizes, final GPR and VR state, raw destination bytes — stopping at
the first divergent write. Sixteen words read as a matrix comes last, and the
identity at zero angles is not an assumption this time.
