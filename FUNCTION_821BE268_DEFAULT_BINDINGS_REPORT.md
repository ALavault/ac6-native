# AC6 default binding initialization at `0x821BE268`

Date: 2026-07-15

## Closed initialization

`0x821CDE88` calls `0x821BE268` for the third `0x1164`-byte logical-input
context at `DAT_826E4EB4 + 0x26854`, with four controller blocks. The main
input update at `0x821CDF08..0x821CE044` polls devices and then updates five
contexts, including that exact third address.

`0x821BE268` iterates four `0x390`-byte blocks and fills their 32 masks:

| Logical slots | Canonical masks |
| --- | --- |
| 0–9 | `20,80,10,40,400,800,1,2,4,8` |
| 10–17 | `10000,20000,40000,80000,100,200,4000,8000` |
| 18–23 | `1000,2000,40,40,10,20` |
| 24–31 | zero |

All values are hexadecimal. Because `0x821CE088` maps raw XInput-A `0x1000`
to canonical `0x20`, the retail default activates logical slots 0 and 23 for
A. Slots 10–17 are marked analog-enabled by `0x821BE568..0x821BE6E4`; their
physical masks are exact, but pitch, yaw, roll, camera, throttle or brake
meanings are not assigned.

## Consumer audit

The corrected re-agent graph contains 15,333 functions. An exact full-export
search of this context's outputs found:

- `DAT_826E4EB4 + 0x276A0` (just-pressed) in `0x8214C038`,
  `0x821B3870`, and the split `0x821B9048/0x821B9050/0x821B9110` path;
- `DAT_826E4EB4 + 0x276D4` (analog slot 10) in `0x820DB578`;
- no reader of the other outputs tied to an aircraft transform, orientation,
  velocity, engine, or weapon record.

The pressed consumers mutate opaque state or invoke virtual methods. The
analog reader performs a generic threshold/filter decision. None has an
independently proven aircraft receiver. The input-to-aircraft chain therefore
remains open and no native aircraft command is added.

## Native boundary

`function_821be268_default_bindings()` reproduces all 32 masks. SDL Return now
runs raw A through `0x821CE088`, the complete retail table, `0x82215140`, and
the edge logic at `0x82214F88`, while remaining separate from aircraft motion.

Instruction evidence is retained in
`reports/logs/default-bindings-821be268-consumers.log`. Re-agent resolved
`0x821BE268` in dry-run mode without spending an LLM call.

The follow-up raw/analog/frame-order audit is recorded in
`FUNCTION_821CE088_ANALOG_RECEIVER_REPORT.md`. It proves the complete
raw-to-logical mapping order but finds no aircraft/camera receiver and no
consumer for the LB+RB or trigger-pair chord masks. The next exact edge remains
the object identity behind an indirect event/virtual consumer, or the pointer
handoff by which flight code reads this context. Only a subsequent write into
proven player-aircraft or camera state permits a real native command.
