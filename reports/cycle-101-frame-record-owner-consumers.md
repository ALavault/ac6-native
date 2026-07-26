# AC6 owner and consumers of the entry-9 frame-record array

Date: 2026-07-17

## Scope

This pass follows cycle 100's three jump-table cases. It qualifies the
container returned by the frame-list helpers and the two immediate consumers
that read the per-record state. It does not assign aircraft, flight, camera,
velocity, or spawn semantics to the records.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Source/list helpers

The corrected headless project gives three related helper boundaries:

- `0x82097400(context)` returns the pointer stored at
  `context + 0x264 + 0x18` when the owner field is non-null;
- `0x820973d0(context)` reads the byte at the first pointed record through
  `context + 0x264 + 0x0c`;
- `0x82097440(context)` reads the byte at the first pointed record through
  `context + 0x264 + 0x18`.

The helpers are therefore bounded list accessors. They do not return a
semantic unit count by themselves and do not identify the pointed records as
aircraft.

## Frame traversal record layout

At `0x8226ecb0`, the owner returned by `0x82097400` supplies a pointer table at
its `+0x04`. The traversal indexes that table with an 8-byte stride and reads
the selected source record's dispatch byte at `+0x2c`. The source record is
then converted into an output/state record in the context-relative array
beginning at `context + 0x58`, with a 0x44-byte stride.

The traversal initializes the output record fields at offsets `+0x00`,
`+0x04`, `+0x08`, `+0x0c`, `+0x10`, `+0x14`, `+0x18`, `+0x1c`, `+0x20`,
`+0x24`, `+0x28`, `+0x2c`, `+0x30`, `+0x34`, `+0x38` and `+0x3c`. It copies the
source record float at `+0x1c` into output `+0x0c`, then uses the source
dispatch byte at `+0x2c` for the bounded jump table. The table cases documented
in cycle 100 write additional state into this same 0x44-byte output stride.

This is an exact layout/dispatch boundary. The field names remain offset
qualified because the source record type and the meaning of its dispatch
values are not proven.

## Immediate consumer: `0x8226c388`

The routine at `0x8226c388`:

1. updates the context status at `+0x04` and mirrors `context + 0x64` to
   `context + 0x288`;
2. clears bit 0 in the context flags at `+0x7c`;
3. calls `0x82273560` with the context-owned external object;
4. obtains the bounded record count through `0x82097440`;
5. scans `context + 0x58` with a 0x44-byte stride;
6. for a record whose `+0x04` carries the observed bit-2 gate, writes
   `context + 0x64` to that record's `+0x20` and calls `0x82269f88` with the
   record index.

This proves a state-record consumer and a `+0x20` writer. It does not prove
that the record represents a unit, an aircraft, or a world transform.

## Immediate consumer: `0x8226f230`

The post-frame routine at `0x8226f230` also obtains the bounded count through
`0x82097440` and walks the same `context + 0x58` array with a 0x44-byte stride.
For each record it checks the observed state bit, calls `0x8226ace0` with the
record index and an enable value, and, when the record's `+0x24` value is at
most one, calls the same routine again with the disable value. The routine
then continues with context/global state and a separate resource-table loop.

The two calls establish a state transition boundary for the frame records;
they do not establish a flight update or a camera operation. The resource
table loop is kept separate from the record classification.

## Shared state-toggle helper: `0x8226ace0`

The `0x8226ace0` target used by `0x8226f230` is now bounded independently. It
rejects a negative record index and obtains the same count through
`0x82097440`. It reads the record's word at `context + 0x58 + index * 0x44 +
0x08`; values `0xfe` and `0xff` are rejected. Other values are used as a
zero-based lookup key after adding one, through the table rooted at
`DAT_826e4eb4 + 0x2d3b4`.

For a non-null table result, the helper reads the mapped object's word at
`+0x70`. The enabled path preserves that word and sets the observed mask
`0x82`; the disabled path clears only the observed `0x02` bit. Both paths
forward the resulting word, the mapped object and literal `1` to the code
entry at `0x822a4f98`.

The initial reading of that entry as a non-returning wrapper was incorrect.
Read-only headless disassembly shows `0x822a4f98` loading the link register and
branching to `0x823864f4` before continuing at `0x822a4fa0`. The target
`0x823864f4` is a shared Xenon save-register helper (`std r27` through `r31`,
save the link register, return); it is not a runtime service. The body fragment
from `0x822a4fa0` through the branch to the shared restore helper at
`0x82386544` performs additional object/table work, but its owning function
boundary and semantic contract are not yet recovered. The call therefore
remains an unresolved object-state/service edge, not evidence of flight,
camera, spawning, or a non-returning operation.

This proves an indexed frame-record-to-runtime-service status transition. It
does not prove that the table key is an aircraft ID, that `+0x70` is a flight
state, or that the service performs movement or spawning.

## Decision

- Keep the source-record pointer table, dispatch byte, output stride, and
  consumer offsets as address-qualified evidence.
- Do not add aircraft or spawn names to the native API from this pass.
- Do not treat the `+0x20` write or the `0x8226ace0` calls as a position or
  flight contract.
- The next useful static join is the owner/type of the pointed source records
  and the owning function boundary for the body fragment after `0x822a4f98`;
  `0x823864f4` itself is compiler support and should not be chased as a game
  service. A dynamic trace is useful later for semantics but is not required
  now.

## Validation

- Read-only Ghidra headless with `DumpRange.java`, `DecompileAt.java` and
  `ReferencesTo.java` against the corrected project.
- Sequential headless access only; no project writer was run.
- AC6 native CTest remains **41/41**.
- No Xenia, Wine, PCSX2, VNC, GUI, or human session was used.
