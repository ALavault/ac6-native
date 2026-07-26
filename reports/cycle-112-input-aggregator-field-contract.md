# AC6 input aggregator field contract

Date: 2026-07-17

## Scope

This pass qualifies the register and field contract of the input update helper
at `0x82215418`. It is intentionally limited to static layout and control
flow. It does not assign keyboard, aircraft, camera or flight meanings to
individual bits.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Entry and aggregate arrays

The entry begins with `r3` as the aggregate state pointer and saves it in
`r31`. It initializes two 32-element `float` ranges from the global value at
`0x8200082c`:

```text
state + 0xe58 .. +0xed7   <- 32 copies of [0x8200082c]
state + 0xed8 .. +0xf57   <- 32 copies of [0x8200082c]
```

The loop is explicit: `r10 = 0x20`, the first destination is `state+0xed8`,
and each iteration stores once at `r11-0x80` and once at `r11`, then advances
`r11` by four bytes.

The scalar state transitions are:

```text
state +0xe48 = old state +0xe44
state +0xe44 = 0
state +0xe4c = 0
state +0xe50 = 0
state +0xe54 = 0
```

These fields remain offset-qualified; no gameplay names are assigned.

## Four device iterations

The device loop uses two independently bounded sequences:

```text
device record:  state + 0x04 + (index * 0x390)
external record: 0x826edb98 + (index * 0xa0)
```

The external end address is `0x826ede18`, so the `0xa0` stride yields four
iterations. The active test reads byte `device+0x04`. For an active device the
helper call is:

```text
FUN_82215140(device, external_record, state+0xe44)
```

The same iteration then calls `FUN_82215210` with the state-derived output
ranges and the external record. The exact register setup is:

```text
r3 = device
r4 = external_record
r6 = state + 0xe58
r7 = state + 0xed8
r8 = state + 0xf58
r9 = state + 0xfd8
```

`FUN_82215140` consumes the external record's pointer/mask at `+0x08` and
combines it with four 32-bit words at device offsets `+0x08`, `+0x0c`,
`+0x10` and `+0x14`. It writes selected bits into the output word at `r5`
(`state+0xe44`). Those operations establish a digital aggregation contract,
not the meaning of the individual bits.

After the four records, `FUN_82214F88(state)` performs the final edge/repeat
stage. No direct aircraft, camera, weapon or flight-state write is present in
this helper.

## Dispatch-table relationship

The data-driven table still contains:

```text
table base  0x82080c40
entry       0x82080c78 = 0x82215418
metadata    0x82080c7c = 0x40002d03
next entry  0x82080c80 = 0x822154d0
```

The table word has no ordinary static code reference in the corrected Ghidra
project. It remains an indirect/runtime dispatch boundary; the metadata is
not treated as a semantic name or a proven input schema.

## Decision

Keep the following names and status:

```text
state +0xe44/+0xe48/+0xe4c/+0xe50/+0xe54  raw input aggregate fields
state +0xe58..+0xed7                         32-float output range A
state +0xed8..+0xf57                         32-float output range B
state +0xf58/+0xfd8                          downstream helper ranges
device +0x04                                 active-byte candidate
device +0x08..+0x14                          digital words, semantics unknown
```

Do not rename these fields to pitch, roll, throttle, yaw, missile or camera
controls. Do not add a native flight receiver from this evidence alone.

## Validation and limits

- `DumpRange.java` qualified the entry, four-record loop, strides and helper
  arguments.
- The table values were re-read with `DumpU32Range.java`.
- No generated Xenon output, source, retail asset or emulator state changed.
- No GUI, Wine, VNC, Xenia or human session was started.
- The native AC6 CTest gate remains **41/41**.
