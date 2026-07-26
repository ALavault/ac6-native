# Entry-9 first Tcam GYZ and NFIC CUT structures

Date: 2026-07-15

## Scope

This pass deepens the already proven resolution of
`Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop`. It recovers bounded record tables
from both the resolved resource and its adjacent `NFIC CUT` payload. Names in
the event dictionary are file evidence; no XEX routine is renamed from them.

## GYZ record table

The first resource remains at entry-9 absolute offset 36,953,728, with a
4,656-byte outer payload and a 4,592-byte `GYZ` region beginning at outer
offset `0x40`.

The `GYZ` region has:

- a `0x20`-byte inner header;
- content tag `0x00011e00` at inner offset `0x20`;
- content identifier `0x9e7562af` at `0x24`;
- record count at `0x2c`;
- record-table offset at `0x30`, observed as `0x50` throughout the corpus;
- fixed record stride `0x30`.

The first `Tcam` table has three records. Its first record reports 121 elements
and two bounded neutral data offsets, `0xe0` and `0x1004`. The second reports
121 elements with offsets `0x870` and `0x10f6`; the third reports one element
with offsets `0x1000` and `0x11e8`. These offsets are relative to the `GYZ`
region and remain semantically neutral.

`MopGyzView` now validates this table in addition to the outer wrapper. Across
all 553 resolved resources it bounds 3,105 fixed-stride records and both data
offset fields of every record.

## NFIC CUT chunk table

The adjacent 39,352-byte state payload remains at entry-9 absolute offset
36,914,048. After its 16-byte `NFICCUT\0` header, it contains nine bounded
chunks, each encoded as a big-endian 16-bit tag, zero 16-bit reserved field,
32-bit payload size, and payload:

| Tag | First payload size |
| --- | ---: |
| `0x3000` | 8 |
| `0x3010` | 24 |
| `0x3020` | 848 |
| `0x3021` | 428 |
| `0x3022` | 156 |
| `0x3030` | 72 |
| `0x3031` | 40 |
| `0x3040` | 37,464 |
| `0x3041` | 224 |

All 44 entry-9 `NFIC CUT` siblings satisfy this nine-chunk layout.

## First verifiable cut sequence

Chunk `0x3041` is a bounded identifier/string dictionary. In the first state
payload it proves these exact associations:

- `0x1001` -> `MoveCamera`;
- `0x8001` -> `CutStart`;
- `0x8002` -> `FrameStart`;
- `0x8003` -> `FrameTerminate`;
- `0x8004` -> `CutTerminate`.

Chunk `0x3040` is a TLV event stream with a 16-bit identifier, zero 16-bit
reserved field, 32-bit payload size, and bounded payload. Its first records
are:

1. relative NFIC offset `0x678`: `0x8001` (`CutStart`), zero-byte payload;
2. relative offset `0x680`: `0x8002` (`FrameStart`), four-byte payload with
   big-endian value 1;
3. relative offset `0x68c`: `0x1001` (`MoveCamera`), eight-byte payload with
   two still-neutral words `0x00010000`, `0x00010000`.

Thus the first file-level cut state and camera event are now directly verified.
The first stream has 2,402 records excluding its zero terminator: one
`CutStart`, 120 `FrameStart`, 120 `MoveCamera`, 120 `FrameTerminate`, one
`CutTerminate`, and other dictionary-tagged records whose meanings remain
outside this slice.

Across all 44 state payloads, the native diagnostic validates 169,908 event
records and 549 non-terminal symbol records. Every state dictionary contains
the four cut/frame names above; the first state also supplies the exact
`MoveCamera` association used here.

## Claim boundary

This closes a serialized mission-cut sequence, not the XEX consumer. No direct
reference to `NFIC`, `GYZ`, or these string identifiers has yet joined the raw
payload to an exact non-virtual executable leaf. The nearby address-based
campaign routines therefore retain their address names. Re-agent was not used
because there is no new exact executable leaf to translate.

## Validation

- GCC ASan+UBSan: 13/13 tests passed;
- Clang ASan+UBSan: 13/13 tests passed;
- MinGW-w64 x86-64: GYZ tests, NFIC CUT tests, and the entry-9 diagnostic build
  as PE32+ console executables;
- malformed magic, chunk sizes, reserved fields, event lengths, symbol
  termination, record-table bounds, and record data offsets are rejected;
- no i686 target was built.
