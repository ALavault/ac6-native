# AC6 entry-9 unit-frame dispatch cases

Date: 2026-07-17

## Scope

This pass follows the frame traversal associated with the entry-9 unit
collection. It classifies three dispatch cases without assigning aircraft,
flight or camera names to opaque records.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Traversal boundary

`0x8226ECB0` iterates the collection count and object pointers, applies the
observed object-state masks at `+0x118`, then builds per-frame auxiliary state
through the `0x820973A0`, `0x820973D0`, `0x82097400` and `0x82097440` helpers.
The dispatch byte at the frame record's `+0x2c` selects a bounded jump-table
case. The case targets are not proven standalone C++ methods; some are inline
labels reached with live registers from the traversal.

## Case `0x8222CCD0`

This case writes a contiguous state block relative to the live dispatch-state
base (`r31`):

```text
+0x830, +0x834, +0x838, +0x83c, +0x840, +0x844  float/state fields
+0x848, +0x85c, +0x84c, +0x868, +0x86c                 flags/masks
```

It tests the absolute values of four floats against `DAT_82069C2C`, gates the
result with source masks at the other live base's `+0xe44` and `+0xe50`, can
clear the four float fields, and calls `0x82387A94` before returning to the
dispatch continuation. This is a concrete per-frame state mutation, but the
register-preserved bases and the surrounding jump-table ABI do not prove
whether the fields are motion, animation, effects, or another subsystem.

The apparent final call is not a downstream state consumer. Headless
disassembly shows `0x82387A94` loading `f28` through `f31` from the saved
register area and returning; it is a shared floating-point epilogue helper.
The state block therefore remains the last direct effect established by this
case.

## Case `0x8222B740`

When its inherited condition is false, this case calls `0x82257DB0` with the
context-owned pointer at `DAT_826E4EB4 + 0x36240`, the live dispatch arguments,
and a literal `4`. `0x82257DB0` is a non-returning wrapper to `0x823864F4`.
The call is therefore recorded as a runtime service/event boundary, not as a
flight update.

## Case `0x82227378`

This case iterates ten entries beginning at the live object base `+0x90`.
For each non-null entry with a nested record at `+0x1c`, it enumerates bounded
records using `0x822C67E8`, computes a checked `0x10`-stride payload pointer,
and forwards it to `0x822272D8`. The `param_4 == -1` path scans all ten entries;
the other path selects `(param_4 + 0x24) * 4`.

The loop proves a nested per-frame collection operation. It does not contain a
direct write to a known aircraft, camera, velocity, weapon, or spawn object.

## Decision

- Preserve all three cases as address-qualified dispatch evidence.
- Do not create native flight helpers or assign semantic field names from
  offsets alone.
- The next static join is the owner/type of the jump-table frame record and
  the proven consumers of the state block after `0x82387A94`.
- A dynamic trace would improve the semantic classification later, but is not
  required for the current static work.

## Validation

- Read-only Ghidra headless: `DumpRange.java`, `DecompileAt.java`,
  `ReferencesTo.java`, and `DumpU32Range.java`.
- AC6 native CTest: **41/41**.
- No Xenia, Wine, PCSX2, VNC, GUI or human session was started.
