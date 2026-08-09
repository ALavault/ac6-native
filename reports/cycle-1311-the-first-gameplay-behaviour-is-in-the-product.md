# Cycle 1311 — the first gameplay behaviour is in the product

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6` for every address cited.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- **Product C++ changed**, for the first time this session.

## What was written

| file | what |
|---|---|
| `include/ac6/retail_input.h` | the derivation, and the types |
| `src/retail_input.cpp` | the three rules |
| `tests/retail_input_tests.cpp` | the rules at their boundaries |

The header carries the whole chain instruction by instruction — the three-line
wrapper at `0x823911C0`, the `addi r4,r31,0x44` that fixes the `XINPUT_STATE`,
the axis loop at `0x8234D144`–`0x8234D1A0` with its table at `0x8201250C`, the
button arithmetic at `0x8234D3A4`–`0x8234D3CC`, the `li r5,0x40` memcpy at
`0x8234D0A0`, and the `cntlzw` that turns `[out+0x04]` into the return value —
so the source cites what it derives from rather than pointing at a report.

Three behaviours are ported and nothing else is:

- `split_axis` — `pos = raw, neg = 0` for `v >= 0`, else `pos = 0, neg = -1 - v`;
- `button_edges` — pressed, released, complement and current, over 32 bits;
- `decode_snapshot` — the `0x40` block, big-endian, offsets stated in device
  coordinates so they can be checked against the listing.

Plus `capability_code`, the five-way map at `0x8234D0B8`.

**What is deliberately absent**: `0x8234D2B0` and its float, `0x82343A90`,
`0x82343AD0`, and anything the 744-instruction consumer `0x821CAA50` does. None
of those has been read, and the header says so where a reader will meet it.

## The tests, and what they are for

Not that it compiles. Each assertion is a rule at a point where a *wrong* rule
and the right one give different answers:

- `-32768`: `-1 - v` is `32767`, and a port computing it in 16 bits overflows.
  Also `-1`, where the negative half is `0` and a naive `-v` would give `1`.
- `0`: `blt` sends zero to the non-negative arm, so the positive half is `0` and
  the negative half is `0` — two zeros, from the branch rather than by accident.
- The complement at `+0x20`: retail complements a **zero-extended halfword**, so
  `~0x8000` is `0xFFFF7FFF` and not `0x7FFF`. A port that masked to 16 bits
  would pass every other test here and fail this one.
- Holding is not pressing: `(prev ^ cur) & cur` is zero when nothing changed.
- Every offset in the table is inside the `0x40` copied and none reaches
  `device+0x44`, so the snapshot cannot accidentally read the kernel structure.
- And the join between the two stages: for the decoded block, each axis pair
  equals what `split_axis` would produce from the raw value beside it.

The axis split is also exercised over **all 65,536 inputs**, checking that the
two halves are never both non-zero and never exceed `32767`.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28   (was 27)
contract_artifacts=pass cited=31 match_head=31
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
```

The suite grew by one, and `ac6-cpp-complexity` — which runs over the whole
source tree — accepted the new files without a waiver.

## Not established

- Everything the header names as unread, above.
- Whether the snapshot the product decodes matches a snapshot retail produced.
  The rules are derived and tested; **no retail bytes have been run through
  them**, because the poll needs a live pad and the harness has no XAM. The
  micro-execution harness could execute `0x8234D110` and `0x8234D378` on a
  synthetic device — they are scalar and inside one register family, which is
  exactly what cycle 1306 said the instrument is good for. That is not done yet.

## Gates this does not yet pass

There is **no contract entry**. Adding one needs a `native-test` artefact — the
convention is a JSON the test writes, as `texture_decode` does with
`ntxr-decode.json` — plus the hashes, and `tools/refresh_contract_evidence.py`
run after `ctest`. Doing that carelessly is how a contract ends up citing a file
that describes a different run, which `CLAUDE.md` records happening once. It is
the next cycle, on its own.

## Next

Two things, in order. Emit the native-test artefact and add `retail_input` to a
contract, so the gate reads it. Then micro-execute `0x8234D110` and `0x8234D378`
against the port — a differential the instrument *can* certify, which would make
this the first behaviour in the campaign backed by retail instructions executing
rather than by a reading alone.
