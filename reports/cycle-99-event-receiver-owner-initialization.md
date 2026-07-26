# AC6 event receiver owner initialization

Date: 2026-07-17

## Target identity

- Target: AC6 Xbox 360 PAL `default.xex`
- Platform/ABI: Xbox 360 Xenon PowerPC, big-endian guest
- XEX SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Ghidra project: `ghidra-projects/ace-combat-6-corrected`
- Analysis mode: read-only, no reanalysis

## New static closure

The complete initialization function `0x820DBF30` provides pointer identity
for the event receiver path previously reached from `0x820DB500`:

1. It stores the context-owned object pointer in the context's `+0x24` path.
2. It allocates `0x150` bytes through the context allocator and calls
   `0x8237EDB0` with the context-owned arguments and the `0x3c` update quantum.
3. It stores the returned event receiver at context offset `+0x04`.
4. When the context feature flag is set, it allocates a separate `0x180`-byte
   object, initializes it with `PTR_Function_820E0298_8205A924`, and calls
   `0x8237E4B8` to store a receiver callback/data pointer at receiver `+0xe8`.

The receiver constructor wrapper `0x8237EDB0` is a tail call to
`0x823864E4`; the setter `0x8237E4B8` is a two-argument store:

```text
receiver + 0xe8 = callback_or_data
```

This confirms the event receiver and its optional callback are created in one
owner initialization flow. It does not identify the callback as flight,
camera, weapon or campaign logic.

## Interface table qualification

The same initialization flow allocates a `0x54`-byte object and writes
`0x8205A8EC` as its first word before clearing its fields. The table at
`0x8205A8EC` therefore has a concrete object/vtable-like owner, rather than
being an unowned constant table. Its `+0x2c` entry is `0x820D9A28`, while
`+0x28` is `0x820D99F8`.

Both entries lie inside the complete `0x820D99C0..0x820D9B38` graphics
shadow-state flow. That flow copies five `float4` groups to object offsets
`+0xaa0..+0xaec` and emits through `0x821E24D8`. The entries are therefore
valid dispatch-table evidence but still require the actual entry ABI before
they can be modeled as ordinary standalone methods.

## Boundary

- The context-to-receiver pointer identity is now stronger than a matching
  offset search.
- The optional callback object is identified by allocation size and its
  initialized table, but no semantic name is assigned.
- The `+0xdc` interface handoff remains a dispatch-ABI boundary; the graphics
  shadow-state target is not a proven gameplay owner.
- No native flight or camera helper is added.
- A dynamic target/calling-convention trace would be useful later, but static
  work can continue by identifying the owner of the `0x54`-byte object and its
  caller-side virtual slot setup.

## Validation and limits

- Read-only headless Ghidra: `DumpRange.java`, `DecompileAt.java`,
  `ReferencesTo.java`, and `DumpU32Range.java`.
- AC6 native CTest from the same checkpoint: **41/41**.
- No Xenia, Wine, PCSX2, VNC, GUI or human session was started.
- This report does not prove campaign activation, post-CUT transition or
  in-flight behavior.
