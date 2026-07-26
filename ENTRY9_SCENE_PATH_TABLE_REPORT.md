# DATA.TBL entry 9 Scene path tables

Date: 2026-07-15

## Retail mapping

All 44 signature-classified `Scene` payloads in decoded physical entry 9 are
now mapped through their complete FHM ancestry. They occur only under two
top-level children:

- child 22: 32 payloads, 44,672 bytes, 349 path records;
- child 23: 12 payloads, 26,112 bytes, 204 path records.

The deterministic mapping is
`reports/entry9-scene-inventory.csv`. Each row records the full dotted FHM path,
top-level child, depth, immediate parent absolute offset, member-local offset,
entry-9 absolute offset, size, path-record count, and first path. There are 45
lines including the header.

The first mapped records are:

- `22.1.0.2`: absolute offset 36,994,416, size 2,304, 18 records, first path
  `Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop`;
- `23.1.0.2`: absolute offset 40,581,888, size 3,968, 31 records, first path
  `Scene/dd01_02a/dd01_02a_01/Tcam__cut01.mop`.

Provenance:

- decoded entry-9 size: 42,446,032 bytes;
- decoded entry-9 SHA-256:
  `cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05`;
- Scene CSV size: 4,534 bytes;
- Scene CSV SHA-256:
  `dde381cb2771183d0bed72b510b82e32bd591f99ba381a08565f5f42ba026c5c`.

A second generation compared byte-identical with this CSV.

## First structurally proven format

The payload has no additional header before its first record. Its exact retail
structure is a non-empty sequence of fixed `0x80`-byte records. Each record is
a NUL-terminated byte string beginning with `Scene/`; therefore the record
count is exactly `payload_size / 0x80`.

The 44 retail payloads all satisfy this contract and contain 553 records. The
observed `.mop` suffix and names such as `Tcam`, `Tunit`, `Tlod`, and
`PostEffect` are retained as path data. They are not yet assigned executable
semantics.

The native `ScenePathTableView` exposes bounded `count()` and `path(index)`
access. It rejects empty payloads, sizes not divisible by `0x80`, missing NUL
terminators, invalid indices, and records without the `Scene/` prefix.

## Consumer boundary

The current static XEX proof does not yet connect top-level DPL child 22 or 23
to a concrete non-virtual Scene consumer. The nearby three-pointer helpers
`0x82291f88`, `0x82291fa8`, and `0x82291fb8` describe a different offset-table
container and must not be conflated with these flat `0x80` path tables.

Accordingly this pass closes the first structurally provable Scene data format,
but does not claim mission-state activation, `.mop` loading, scene traversal, or
render submission. No renderer-oriented function name is introduced.

## Validation

- GCC ASan+UBSan: 11/11 tests passed;
- Clang ASan+UBSan: 11/11 tests passed;
- MinGW-w64 x86-64: isolated Scene tests and the entry-9 diagnostic compile as
  PE32+ console executables;
- no i686 target was built.

No re-agent generation was used: the remaining XEX frontier is virtual or
unjoined, while the exact non-virtual helpers inspected in this pass parse a
different container.
