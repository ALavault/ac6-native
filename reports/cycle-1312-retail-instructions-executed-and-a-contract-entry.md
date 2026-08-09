# Cycle 1312 — retail instructions executed, and a contract entry

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** The micro-execution ran retail instructions on a
  synthetic device: no console, no bridge, no game.
- Product C++ changed.

## The differential

Cycle 1311 shipped `retail_input` derived and tested, and said so plainly:
*derived and tested is not verified*. The tests check the port against the rules
I wrote down, and a misread branch would make both agree and both be wrong.

`tools/audit_retail_input_microexec.py` closes it. Each case executes
**`0x8234D110` or `0x8234D378` themselves** and compares every byte they write
against what the port says they write.

The device is split into three regions so one object can be both pre-filled and
write-detected — the output block `+0x00…+0x43` poison, the `XINPUT_STATE` at
`+0x44` a literal, the tail zeroed — with `device+0x74` seeded through a fourth
literal region when the previous button mask is non-zero.

**12 cases, 168 fields, no divergence.** Six axis vectors including `0`, `±1`,
`±32767`, `-32768`; six button pairs including a rising edge under a held
button, a falling edge, no change under `0xFFFF`, and `0xA5A5 → 0x5A5A`.

`0x8234D1B8` and `0x8234D210` are stubbed, recorded in the report, because
neither has been read. `callee_entries=1` on the button cases is the axis stage,
which `0x8234D378` calls first — so those cases assert its writes too, and the
expectation includes them.

This is the first behaviour in the campaign whose native rules are backed by
**retail instructions executing** rather than by a reading alone.

## The contract entry

`analysis/contracts/mission01-playable-gate-v1.json`, built from v3 the way v4
was: the eight JF behaviours plus one. `retail_input` carries four pieces of
evidence — `static`, `native-test`, `microexec`, `derivation` — where the schema
requires three.

The auditors, in the order `CLAUDE.md` sets:

```
mission01_final_gate  (playable-v1)  JF=pass open=none
mission01_final_gate  (final-v3)     JF=pass open=none
ctest                                28/28
git status                           after ctest, not before
refresh_contract_evidence            paths=4 uncited=0
contract_addresses                   cited=144 supported=144 unsupported=0
contract_derivations                 behaviours=27 gaps=0 multiple_derivations=0
```

`contract_artifacts` failed first, on `not_committed`, for the two artefacts this
cycle created — which is the check doing its job, and the reason it exists.

The derivation validator passes because the header names every one of the twelve
addresses the behaviour claims, in the file itself. That is the rule that makes
a contract entry mean something: the contract cannot vouch for itself, so the
native source has to carry its own origin.

## Two tasks closed as stale, and why

- *Settle the `+0xC0` displacement collision.* Opened at cycle 1302 for
  `0x822A1E80`, which cycle 1306 stopped on the `vpermwi128` question. It cannot
  be settled while the routine it belongs to cannot be executed correctly, and
  reopening it would be reopening that thread.
- *Partition the CALLOTHER census by the player update path.* Superseded in
  method by cycle 1304: counting CALLOTHERs measures only what the module
  declined to implement loudly, and the census by mnemonic replaced it. The
  player update path is also not identified, so the partition has no subject.

Both are recorded here rather than silently dropped.

## Not established

- What the game does with the snapshot. `0x821CAA50`, 744 instructions, unread.
- `0x8234D2B0` and its float, `0x82343A90`, `0x82343AD0`.
- Whether a real pad produces snapshots in the ranges the port assumes. The
  differential feeds synthetic devices; nothing here has seen hardware.

## Gates

```
mission01_final_gate (v3 and playable-v1)  JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28
contract_addresses=pass cited=144 supported=144 unsupported=0
contract_derivations=pass behaviours=27 gaps=0
retail_input_microexec=pass 12 cases, 168 fields
tools/tests: Ran 72 tests, OK
```

## Next

`0x821CAA50`. It holds seven of the eleven call sites into the input API and it
is what turns a snapshot into whatever the flight code consumes. It is 744
instructions, so it is a read rather than a scan, and it is the last scalar step
before the flight maths — where the vector instrument's limit is waiting.
