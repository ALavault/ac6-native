# AC6 event receiver constructor ABI

Date: 2026-07-17

## Scope

This static pass closes the owner-to-interface handoff for the event receiver
used by `0x8237e4c0`. It records the constructor's register-to-field contract
and the qualified vtable entries, but does not assign a gameplay meaning to
the interface or to any event code.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

The corrected Ghidra project was queried in read-only headless mode with
Ghidra 12.1.2 and `DumpRange.java`, `DumpU32Range.java` and
`DecompileAt.java`. The loader splits the contiguous constructor body into
several function records, so the raw PPC range is the authoritative boundary
for the register mapping.

## Constructor entry and register mapping

The contiguous body beginning at `0x8237edb0` receives a receiver object in
`r3` and stores the following parameters:

| Register | Receiver field | Operation |
| --- | ---: | --- |
| `r4` | `+0xdc` | stores the opaque interface pointer |
| `r6` | `+0xe0` | stores a pointer/value; if non-null, calls its first virtual slot with the receiver |
| `r7` | `+0xe4` | stores a pointer/value; if non-null, calls its first virtual slot with the receiver |
| `r8` | `+0xe8` | stores optional callback/data |
| `r9` | `+0xec` | stores an optional pointer; if non-null, calls its first virtual slot with the receiver |
| `r10` | `+0x104` | stores a scalar used by the later update path |
| stack byte `0xf7` | `+0x10c` | stores a byte value |

The constructor also:

- writes receiver bytes `+0x00 = 0`, `+0x01 = 0`, `+0x02 = 1`;
- clears `+0xf4` and `+0xfc`;
- invokes the table/reset helper at `0x8237ecc0` on `receiver + 0x08`;
- stores `r6` at `+0xe0` and `r7` at `+0xe4` before their optional callbacks;
- stores `r8`/`r9` at `+0xe8`/`+0xec` and invokes the optional `r9` callback;
- enters `0x8237e458` with the receiver and a constructor-side argument before
  storing `r10` and the stack byte.

The exact stores are visible at `0x8237edfc..0x8237ee8c`; the `+0xdc` store is
`0x8237ee04: stw r28,0xdc(r31)` after `r28` is copied from `r4`.

## Interface table and dispatch shape

The interface pointer is initialized from a table whose first word is
`0x8205a8ec`. The table entries relevant to the receiver's callback path are:

```text
table +0x28 = 0x820d99f8
table +0x2c = 0x820d9a28
```

The raw range `0x820d99c0..0x820d9b38` shows that `0x820d99f8` and
`0x820d9a28` are interior addresses in one state-emission routine, not normal
function starts:

- `0x820d99f8` loads an immediate and branches to `0x82336cc0`;
- `0x820d9a28` is an `or` instruction in the same routine;
- the routine copies five groups of four 32-bit values from the input pointer
  into object offsets `+0xaa0..+0xaec` and emits through `0x821e24d8`.

This explains why the receiver's indirect slot `+0x2c` must not be modeled as
a conventional standalone C++ method until the dispatch ABI is recovered.
The object/vtable relationship is real; the target's semantic role is not.

## What this closes

- The `+0xdc` interface is not an arbitrary guessed field: it is the `r4`
  constructor argument of the event receiver.
- The neighboring fields `+0xe0`, `+0xe4`, `+0xe8`, `+0xec`, `+0x104` and
  `+0x10c` have exact construction stores and callback guards.
- The table entries used by the receiver are verified to belong to an
  interior-address dispatch/state-emission family.

## What remains open

- the producer and semantic owner of constructor `r4`;
- the ABI of the interior entry at table `+0x2c`;
- the meaning of the scalar and callback fields;
- the table-dispatched event vocabulary and any relation to flight/camera
  state;
- the post-CUT campaign and playable-aircraft owner.

No native flight or input helper is added. This is a reusable ABI/evidence
contract and requires no human session.

## Validation

- `DumpRange.java` confirmed the constructor stores and the interior dispatch
  addresses.
- `DumpU32Range.java` confirmed the table entries at `0x8205a8ec`.
- `DecompileAt.java` confirmed the bounded state-emission body where Ghidra
  recognized a function boundary.
- No generated output, source, retail asset or emulator state changed.
- No GUI, Wine, VNC, PCSX2, Xenia or human session was started.
- AC6 native CTest remains **41/41** from the current gate.
