# AC6 event interface provenance

Date: 2026-07-17

## Scope

This static pass identifies the object passed in `r4` to the event-receiver
constructor at `0x8237edb0`. It closes pointer provenance for `receiver+0xdc`
without assigning a gameplay or flight meaning to the object.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Evidence comes from the corrected Ghidra project using read-only headless
`DumpRange.java`, `DumpU32Range.java` and `FindDirectCallsTo.java`.

## Construction path

The initializer body beginning at `0x820dbd90` performs the following bounded
steps after its input and global guards pass:

1. Build a small local record through `0x82238218` and `0x822383d0`.
2. Allocate `0x54` bytes through the repository allocator
   `0x820ed668`; retain the result in `r30`.
3. Store the table pointer `0x8205a8ec` at the new object's `+0x00`.
4. Clear the observed fields `+0x10`, `+0x14`, `+0x18`, `+0x20`, `+0x24` and
   `+0x28`.
5. Allocate a `0x0c` subobject and store its pointer at the outer object's
   `+0x30`; its initialization writes self-links at the subobject's first two
   words when allocation succeeds.
6. Retain the outer `0x54` object in `r29`.
7. Dispatch the outer object's table slot `+0x04` with the scalar `0x3838`.
8. Pass `r29` as `r4` to `0x8237edb0` at call site `0x820dbf78`.

The raw stores are at `0x820dbe24..0x820dbe84`, the slot call setup is at
`0x820dbe94..0x820dbea8`, and the receiver-constructor call setup is at
`0x820dbf58..0x820dbf78`.

## Provenance result

The receiver constructor's `r4 -> receiver+0xdc` field therefore receives the
newly allocated, table-initialized `0x54` object. It is not the allocator
object used by `0x820ed668` and it is not merely an unqualified global pointer.

The object has this evidence-only shape:

```text
outer object (allocation request 0x54)
  +0x00 = 0x8205a8ec
  +0x10/+0x14/+0x18 = 0
  +0x20/+0x24/+0x28 = 0
  +0x30 -> subobject (allocation request 0x0c)
  +0x34 = 0
```

The table slot `+0x04` resolves to `0x820d94b8`. That address is part of the
same interior-address state-emission family documented in cycle 109; it is not
safe to model it as an ordinary method whose `r3` points to the compact outer
object. The constructor-side dispatch ABI remains unresolved.

## What this closes

- `receiver+0xdc` has a concrete allocation and initialization provenance.
- The outer interface object, its table pointer, and its nested `0x0c`
  subobject are target-qualified.
- The call-site join from `0x820dbd90` through `0x820dbf78` to
  `0x8237edb0` is static and reproducible.

## What remains open

- the semantic class of the `0x54` object;
- the actual ABI expected by table slot `+0x04`/`+0x2c`;
- the producer of the event table entries and any relation to flight state;
- post-CUT campaign and aircraft ownership.

No native input or flight helper is added. A future runtime trace may use this
object identity to capture the interface safely, but static work does not
require a human session.

## Validation

- `FindDirectCallsTo.java` confirmed the unique constructor call at
  `0x820dbf78`.
- `DumpRange.java` confirmed allocation, table, subobject and argument stores.
- `DumpU32Range.java` confirmed the table root `0x8205a8ec`.
- No generated output, source, retail asset or emulator state changed.
- No GUI, Wine, VNC, PCSX2, Xenia or human session was started.
- AC6 native CTest remains **41/41** from the current gate.
