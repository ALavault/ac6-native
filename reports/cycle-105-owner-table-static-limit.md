# AC6 owner-table static limit

Date: 2026-07-17

## Target

Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Result

The owner table used by the frame-record path is addressed through the
runtime pointer at `DAT_826e4eb4` and the offset `+0x2d3b4`. A read-only dump
of the static XEX image at `0x826e4eb4` contains zero words; the referenced
table and its entries are therefore initialized outside the static image.
The same pattern affects the global service table at `+0x29c80`.

The caller convergence in cycle 104 proves an object/vtable-shaped contract,
but no concrete vtable, constructor, or object-table entry can be recovered
from the current static image without following runtime initialization.

## Classification

This boundary is now explicitly `needs-dynamic-evidence`:

- static evidence: object pointer, state field `+0x70`, vtable calls and table
  offsets are retained;
- missing evidence: runtime table pointer, concrete vtable identity and the
  callback target selected by `+0x29c80`/`+0x2d3b4`;
- next action: capture one qualified AC6 runtime state later, using the existing
  Xenia/savestate route, when human sessions are authorized.

No aircraft, flight, camera, weapon or spawn semantics are assigned from this
limit. Work can continue on other static targets without losing this AC6
frontier.

## Validation

- Read-only Ghidra headless with `DumpDataWords.java` and caller decompilation.
- No project writer, Xenia, Wine, VNC, GUI or human session.
- AC6 native CTest remains **41/41**.
- `git diff --check`: pass.
