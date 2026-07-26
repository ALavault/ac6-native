# AC6 event receiver payload contract

Date: 2026-07-17

## Scope

This static pass follows the receiver behind the previously unresolved
`0x8237e4c0` edge. It qualifies the payload words and the receiver-side state
updates, but it does not name an aircraft, flight, camera or weapon command.
No Xenia or human run is required for this boundary.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

The corrected Ghidra project was queried read-only with Ghidra 12.1.2 and the
repository's `DecompileAt.java`, `ReferencesTo.java` and
`FindDirectCallsTo.java` scripts.

## Direct-call inventory

The corrected project contains ten direct callers of `0x8237e4c0`:

```text
0x820db1dc
0x820db4e4
0x820db6a4
0x820db6d0
0x820db6f0
0x820db720
0x820db748
0x820db760
0x820db7a8
0x820db7c0
```

They are contained in the event/update helpers around `0x820db1a0`,
`0x820db4c8`, `0x820db578` and `0x820db628`. The caller bodies pass the same
receiver pointer, loaded from an object/context field at `+0x04`, with a
stack-local payload at one of the observed `+0x50`, `+0x54`, `+0x58`,
`+0x60`, `+0x68`, `+0x70`, `+0x78` or `+0x80` locations. The payload values are
not all recovered by the current decompiler output, so no caller-side event
name is assigned.

## Receiver contract

`0x8237e4c0(receiver, payload)` first checks:

```text
receiver[+0x00] == 0
receiver[+0x02] != 0
```

The first payload word is a signed event code. The following cases are
directly visible in the retail body.

### Code 1

```text
receiver + 0x124 = payload[1]
receiver + 0x128 = payload[2]
```

The two values are copied as 32-bit words. Their semantic type is not known.

### Code 5

`payload[1]` is consumed as a signed byte `b`:

```text
word_offset = ((b >> 5) + 0x4b) * 4
receiver[word_offset] |= 1u << (b & 31)
receiver + 0x14c = b
```

The operation is a bit-set in a receiver-owned bitmap and records the byte at
`+0x14c`. It is not safe to call the byte a control, weapon or flight action
without a runtime event trace.

After code 5, the receiver also:

- invokes `0x8237f7d0(receiver + 0xf4)`, whose current body is a non-returning
  handoff to `0x823864fc`;
- invokes an indirect method at `*(receiver + 0xdc)` slot `+0x2c`;
- invokes `0x82381570`, which calls the nested object's virtual slot `+0x134`.

### Code 6

Code 6 performs the same byte-to-bitmap calculation and clears the selected
bit instead of setting it:

```text
receiver[word_offset] &= ~(1u << (b & 31))
```

It records no new byte at `+0x14c`, then performs the corresponding cleanup
path through `0x8237f7d0`, the receiver `+0xdc` slot `+0x2c`, and
`0x82381540` (nested virtual slot `+0x138`).

### Other codes

If `receiver + 0xfc` is non-null and the word at its `+0x18` equals
`receiver + 0x100`, the body calls a table-dispatched handler from
`DAT_8267a4c0[event_code]` via `0x82382e40`. The table entry may be null and
the current static evidence does not establish its owner or event vocabulary.

## What this closes

- `0x8237e4c0` is a guarded receiver-side event/state routine, not an
  unqualified flight function.
- Code 1 has an exact two-word state-copy contract.
- Codes 5 and 6 have an exact byte-indexed bitmap set/clear contract and a
  post-dispatch virtual-call shape.
- The ten direct call edges and their receiver/payload boundary are now
  address-qualified.

## What remains open

- the producer-side values of the stack-local payloads;
- the owner and ABI of the `+0xdc` indirect interface;
- the semantic names of the bitmap and `+0x14c` byte;
- the table owner and entries at `DAT_8267a4c0`;
- any relation to player input, aircraft state, camera state, mission or
  post-CUT flight activation.

The existing input report therefore remains valid: the input aggregator and
this receiver are separate evidence layers. No logical keyboard/controller
bit is renamed from the receiver contract alone.

## Native decision

Do not add a flight or input helper. Preserve this as a generic event receiver
contract and use it to constrain a future dynamic trace or a producer-side
static join. The runtime-initialized owner/table boundaries remain
`needs-dynamic-evidence`; static work can continue without a human session.

## Validation

- Read-only Ghidra headless decompilation completed for `0x8237e4c0`, its
  callback tails and the direct caller families.
- `ReferencesTo.java` and `FindDirectCallsTo.java` agree on the ten direct
  call sites listed above.
- No generated output, source, retail asset or emulator state changed.
- No GUI, Wine, VNC, PCSX2, Xenia or human session was started.
- AC6 native CTest remains **41/41** from the current gate.
