# AC6 player-input dispatch table boundary

Date: 2026-07-17

## Target identity

- Target: AC6 Xbox 360 PAL `default.xex`
- Platform/ABI: Xbox 360 Xenon PowerPC, big-endian guest
- XEX SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Ghidra project: `ghidra-projects/ace-combat-6-corrected`
- Analysis mode: `-readOnly -noanalysis` with versioned headless scripts

## Question

Determine whether the input aggregation helper at `0x82215418` has a direct
flight or aircraft consumer, and preserve the distinction between the input
dispatcher and a gameplay receiver.

## Static findings

The direct-call audit found:

| Function | Direct caller | Interpretation |
| --- | --- | --- |
| `0x82215140` | `0x82215470` | configurable digital-mask mapping inside the input update |
| `0x82214F88` | `0x822154A0` | edge/repeat processing at the end of the update |
| `0x821CE088` | none in the corrected static project | dynamic/data-driven input entry remains unresolved |

`0x82215418` is a bounded four-device input aggregator. It clears the current
analog arrays, advances the previous/current state fields, walks controller
records at the observed `0x390` stride, calls `0x82215140` only for active
devices, calls the analog helper at `0x82215210`, and finishes at
`0x82214F88`. The function has no statically proven write to an aircraft,
camera, weapon, or flight-state object.

The helper is also a member of a data-driven function/metadata table. The
exact table entry is:

```text
table base  0x82080c40
entry       0x82080c78 = 0x82215418
metadata    0x82080c7c = 0x40002d03
next entry  0x82080c80 = 0x822154d0
```

The preceding entries use the same address/metadata alternation (for example
`0x82080bd0 = 0x82213758`, `0x82080bd4 = 0x4000e203`). This proves an
indirect-dispatch membership and its adjacent metadata, but not the table
owner, role name, or a flight semantic. No direct code reference to the table
word itself was recovered, so the table consumer remains a runtime/data-driven
boundary.

The body-local calls are therefore sufficient to identify the routine as an
input update helper, not sufficient to identify a player-aircraft receiver.
The earlier receiver audit remains applicable: logical analog outputs reach a
generic event path at `0x8237E4C0`, while no proven consumer currently writes
flight or camera state.

The adjacent interface table at `0x8205A8EC` reinforces the boundary rather
than closing it. Its `+0x28` entry is `0x820D99F8` and its `+0x2C` entry is
`0x820D9A28`; both addresses lie inside the complete
`0x820D99C0..0x820D9B38` flow. That flow copies five `float4` groups into
`+0xAA0..+0xAEC` of a graphics command/state object and emits through
`0x821E24D8`. The table therefore cannot safely be treated as a conventional
standalone vtable without recovering its dispatch ABI; the interior addresses
do not prove gameplay ownership.

## Decision

- Keep `0x82215418` and table metadata classified as `dynamic`/`unknown`
  dispatch evidence, not as a named gameplay method.
- Do not rename logical bits as pitch, roll, throttle, yaw, missile or camera
  controls from this table alone.
- Do not add a native flight helper or alter the SDL shell on this evidence.
- The next static target is the runtime owner/consumer of the dispatch table or
  the receiver behind `0x8237E4C0`, searched by pointer identity and proven
  writes. A reproducible Xenia campaign/flight activation trace would be useful
  later, but is not required to continue this static boundary.

## Validation and limits

- `cmake --build .build/ace-combat-6 -j16`: passed.
- `ctest --test-dir .build/ace-combat-6 -j16 --output-on-failure`: **41/41**.
- Headless Ghidra scripts used: `FindDirectCallsTo.java`, `ReferencesTo.java`,
  `DumpRange.java`, `ListFunctionsRange.java`, `FindU32Any.java`, and
  `DumpU32Range.java`.
- No Xenia, Wine, PCSX2, VNC, GUI or human session was started.

The 41/41 result is a native regression gate only. It does not prove retail
campaign activation, post-CUT transition, flight controls or parity.
