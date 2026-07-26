# AC6 event payload producer matrix

Date: 2026-07-17

## Scope

This static pass joins the ten direct calls to `0x8237e4c0` with the payload
stores immediately preceding each call. It qualifies event-code and field
initialization facts for the corrected PAL XEX, but it does not assign names
such as input, aircraft, weapon, camera or flight event.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Evidence was collected from the corrected Ghidra project with
`DumpRange.java`, `DecompileAt.java` and `FindDirectCallsTo.java` in read-only
headless mode.

## Producer families

The receiver argument is loaded from the same object/context field:

```text
r3 = *(caller_object + 0x04)
r4 = stack-local payload
bl  0x8237e4c0
```

The direct-call matrix is:

| Call site | Payload address | Code written | Other writes proven | Guard before call |
| --- | --- | ---: | --- | --- |
| `0x820db1dc` | `caller + 0x50` | `0` | payload `+0x04 = 0`, `+0x08 = 0` | global flag and caller `+0x08` nonzero |
| `0x820db4e4` | `caller + 0x50` | `0` | payload `+0x04 = 0`, `+0x08 = 0` | helper result path |
| `0x820db6a4` | `caller + 0x58` | `0` | payload `+0x04 = 0`, `+0x08 = r30` | `r30 > 0` and `r29 != 0` |
| `0x820db6d0` | `caller + 0x60` | `5` | payload `+0x04 = r24` as byte | `r24 != 0`, `r26 != 0` |
| `0x820db6f0` | `caller + 0x68` | `6` | payload `+0x04 = r24` as byte | `r24 != 0`, `r26 == 0` |
| `0x820db720` | `caller + 0x70` | `0` | payload `+0x04 = 0`, `+0x08 = r30` | `r3 != 0`, `r30 > 0` |
| `0x820db748` | `caller + 0x50` | `3` | only code store is proven at this site | `r24 != 0`, `r31 == 0` |
| `0x820db760` | `caller + 0x78` | `5` | payload `+0x04 = r24` as byte | `r24 != 0` |
| `0x820db7a8` | `caller + 0x54` | `4` | only code store is proven at this site | `r25 != 0`, `r24 != 0`, `r31 == 0` |
| `0x820db7c0` | `caller + 0x80` | `6` | payload `+0x04 = r24` as byte | `r25 != 0`, `r24 != 0` |

The table reports only stores visible in the bounded producer body. A stack
word not written on a particular path is not treated as initialized merely
because the receiver's ABI accepts a pointer to the payload.

## Exact producer facts

### Code 0

The first two producer families explicitly create a three-word payload
`[0, 0, 0]`. The later two sites create `[0, 0, r30]`, where `r30` is the
sign-extended byte-derived input retained by the enclosing helper. The static
contract does not assign a type or semantic name to `r30`.

### Codes 5 and 6

The producer writes the code word and stores `r24` as a byte in payload
`+0x04`. The receiver consumes that byte for its bitmap set/clear operation.
The code-5 and code-6 pairs are therefore a confirmed set/clear relationship
for the same producer byte, but the bit's domain remains unknown.

### Codes 3 and 4

The current producer sites write only the code word before dispatch. No
additional payload field is proven initialized at `0x820db748` or
`0x820db7a8`. These paths must remain opaque table-dispatch events until the
callee or a complete stack construction proves their payload ABI.

### Code 1

No code-1 producer was found in this ten-call update family. Its receiver-side
copy contract from cycle 107 remains valid, but its producer and owner are
still unresolved.

## Decision

This closes the producer-side event-code matrix for the audited update family:

```text
0 -> four calls
3 -> one call
4 -> one call
5 -> two calls
6 -> two calls
1 -> no producer found in this family
```

Do not map these codes to keyboard/controller actions, aircraft state, camera
state, weapon state or mission transitions. The common receiver object,
`+0xdc` interface, and the data-driven `DAT_8267a4c0` handler table remain
address-qualified but semantically unresolved.

No native flight or input helper is added. This evidence can constrain a
future runtime trace or a second static join without any human session.

## Validation

- `FindDirectCallsTo.java` returned the ten call sites in the matrix.
- `DumpRange.java` confirmed the code and payload stores at each site.
- No generated output, source, retail asset or emulator state changed.
- No GUI, Wine, VNC, PCSX2, Xenia or human session was started.
- AC6 native CTest remains **41/41** from the current gate.
