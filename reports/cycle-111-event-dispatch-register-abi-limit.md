# AC6 event dispatch register-ABI limit

Date: 2026-07-17

## Scope

This pass records the register state immediately before the event receiver's
indirect `+0x2c` call and compares it with the table continuation selected by
the interface object. The purpose is to prevent an unsafe C++ method model;
it does not assign a gameplay meaning.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Receiver-side register state

At `0x8237e4c0`:

```text
0x8237e4d0: r31 = r3                 // preserve receiver
...
0x8237e5bc: r3 = [receiver + 0xdc]   // load interface object
0x8237e5c0: r11 = [r3 + 0x00]        // load table root
0x8237e5c4: r11 = [r11 + 0x2c]       // select slot +0x2c
0x8237e5c8: CTR = r11
0x8237e5cc: bctrl
```

The payload pointer remains in `r4` along these code-5/code-6 paths. Thus the
observable call shape is:

```text
r31 = event receiver
r3  = interface object
r4  = event payload
CTR = interface_table[0x2c]
```

`r31` is a callee-saved register from the receiver's own prologue, not an
argument that a normal C++ call would expose.

## Continuation mismatch

For the interface table rooted at `0x8205a8ec`, slot `+0x2c` contains
`0x820d9a28`. This address is an interior instruction of the common state
emission range `0x820d99c0..0x820d9b38`; the bounded body uses its callee-saved
`r31` value as the object being written and emits groups at offsets
`+0xaa0..+0xaec` before calling `0x821e24d8`.

The event receiver allocation request is only `0x150` bytes, while the
interface object passed in `r3` is the separately allocated `0x54`-byte
object documented in cycle 110. Therefore the current static evidence cannot
reconcile the interior continuation's large offsets with an ordinary
receiver/interface method ABI.

This is a positive boundary, not permission to discard either observation. It
may indicate a custom continuation ABI, a shared caller-owned state object, or
a function-boundary/loader interpretation that needs a qualified trace. The
current project does not distinguish these alternatives statically.

## Decision

Keep the `+0x2c` dispatch as:

```text
dispatch_opaque_needs_abi_evidence
```

Do not:

- expose it as a native C++ method;
- use the `+0xaa0..+0xaec` offsets in the compact receiver model;
- infer graphics, flight, camera or input semantics from the interior target;
- modify generated Xenon output.

This limit is not a reason to request a human session yet. The producer matrix,
receiver fields and interface provenance remain useful static evidence, and
other AC6/native work can proceed independently.

## Validation

- `DumpRange.java` confirmed the live `r31/r3/r4` setup and `bctrl` sequence.
- `DumpU32Range.java` confirmed `0x8205a8ec + 0x2c = 0x820d9a28`.
- The bounded state-emission range was inspected from its raw PPC body.
- No generated output, source, retail asset or emulator state changed.
- No GUI, Wine, VNC, PCSX2, Xenia or human session was started.
- AC6 native CTest remains **41/41** from the current gate.
