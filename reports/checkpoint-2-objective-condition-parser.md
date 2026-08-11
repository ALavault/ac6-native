# Checkpoint 2 — tag-7 objective condition boundary

## Qualification

The source is the PAL `default.xex` (`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`) in the canonical `ghidra-projects/ace-combat-6` project.  The qualified cache is `/tmp/ac6-retail-v2-smoke`, whose content-index digest is `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.

## Binary boundary

The tag-7 consumer at `0x8226E158` dereferences the first child of the step's `pas[8]` payload.  The native parser reads only the six fields established by that consumer:

| offset | field |
|---:|---|
| `+0x00` | big-endian `u16` counter id |
| `+0x02` | big-endian `s16` threshold |
| `+0x04` | `u8` comparison (`0 ==`, `1 <=`, `2 >=`) |
| `+0x05` | `u8` target sub-mission |

Counter ids `0` and `0xffff` are the retail no-condition sentinels.  A tag-7 step without a bounded child, a truncated six-byte record, or an operator outside `0..2` now rejects the scenario instead of becoming an unconditional objective.  Conditions are stored parallel to `step_tags` as `ScenarioSubMission::step_conditions`; `MissionScriptRunner::current_condition()` exposes the current record while leaving counter ownership with the mission state.

## Qualified Mission 07 corpus

Mission 07 is the only campaign scenario currently carrying tag 7.  Its four non-sentinel records are:

| sub-mission | step | id | threshold | comparison | target |
|---:|---:|---:|---:|---:|---:|
| 0 | 2 | 263 | 1 | `==` | 1 |
| 0 | 3 | 264 | 1 | `==` | 2 |
| 1 | 1 | 97 | 1 | `==` | 3 |
| 2 | 1 | 97 | 1 | `==` | 3 |

The all-15 qualified session bootstrap parses this payload before constructing the product session and asserts these exact records.  This closes the binary reader boundary only.  The meanings and producers of the 339 counters, AI/event timing, and the runtime jump path remain open; no counter is fabricated and no mission is marked complete by this report.

## Validation

`cmake --build reconstruction/ace-combat-6/build -j16` passed after the parser change.  The qualified all-15 CTest/session corpus is the required follow-up gate; the existing synthetic parser tests continue to cover non-tag-7 scenarios.
