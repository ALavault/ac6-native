# DATA.TBL entry 9 child-1 consumer chain

Date: 2026-07-15

## Deterministic retail inventory

The campaign selector-1 path resolves to physical `DATA.TBL` entry 9. Decoding
that entry produces a 42,446,032-byte top-level FHM. The native
`ac6-entry9-diagnostic` recursively inventories the decoded file and reports:

```json
{"data_table_entry":9,"decoded_size":42446032,"manifest_rows":1111,"scene_rows":44,"child_index":1,"child_offset":3538944,"child_size":29097984,"child_magic":"MDLP","mdlp_elements":94,"offset_table":4096,"data_base":8192,"consumer":"Function_820A7070"}
```

This is deterministic signature-based inventory, not a claim that all 44
`Scene` records are simultaneously instantiated by the first mission.

## Exact child-1 structure

Top-level FHM child 1 begins at decoded offset `0x00360000`, has size
`0x01bc0000`, and begins with `MDLP`. Its header contains:

- element count 94 at `+0x04`;
- declared size `0x01bc0000` at `+0x08`;
- element-offset table at `+0x0c`, value `0x1000`;
- element-data base at `+0x10`, value `0x2000`.

The XEX helpers use this layout directly:

- `0x8228e988` builds a three-pointer view for the MDLP base, offset table, and
  data base;
- `0x8228e9a8` returns the element count at MDLP `+0x04`;
- `0x8228e9b8` bounds-checks the requested index and returns
  `data_base + offsets[index]`.

The native `Function8228e988View` preserves those offsets but adds declared-size,
table-range, index, and relative-offset bounds. Address-based naming is retained
until the wider object responsibilities are recovered.

The native-only `element_span` extension uses the next offset as an exclusive
bound, rejects descending/unbounded offsets, and uses the declared MDLP end for
the last element. It does not change the reconstruction of the retail helper,
which returns an unbounded pointer tail.

## Inventory of all 94 MDLP elements

All 94 bounded elements begin with an FHM signature. Recursively, they contain
1,748 member rows: 47 nested FHM, 86 `NTXR`, 292 `NDXR`, 381 `MATE`, 942
signature-neutral binary records, and no `Scene`. Thus child 1 is independently
anchored as the geometry/material/texture package branch; the 44 `Scene`
payloads in entry 9 are outside this MDLP child.

The deterministic per-element artifact is
`reports/entry9-child1-mdlp-inventory.csv`. It records each index, relative
offset, bounded size, root kind, recursive row count and typed signature counts.
It has 95 lines including its header.

Provenance:

- decoded entry-9 size: 42,446,032 bytes;
- decoded entry-9 SHA-256:
  `cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05`;
- CSV size: 4,353 bytes;
- CSV SHA-256:
  `ed3a584dbccab05e9666bcdaa1b839241d73853db884dcb540b078b888417b98`.

## Consumer path

At `0x820a70b4..0x820a70d4`, `0x820a7070` invokes object virtual slot `+0x0c`.
The concrete slot is `0x820a85e0`, which selects DPL child 1 and initializes the
local MDLP view through `0x8228e988`.

`0x820a7170..0x820a71c0` iterates all 94 elements with
`0x8228e9a8`/`0x8228e9b8` and sends each pointer to a global service virtual
slot `+0x18`, with literal type `0x98`. `0x820a71c4..0x820a7214` makes a second
complete pass to slot `+0x1c`, again with type `0x98`.

Later, `0x820a7954..0x820a79b4` reads element indices from source-record bytes
`+0x61` and `+0x62`, resolves one or two MDLP elements with `0x8228e9b8`, and
passes them to service virtual slot `+0x10`, still with type `0x98`. The
surrounding function then constructs and updates runtime objects through
additional virtual calls.

This proves a scene/model resource-consumer boundary. It does not yet prove
which service calls upload graphics resources, submit draws, or attach scene
nodes, so those functions remain unnamed rather than being prematurely labeled
as renderer methods.

More narrowly, the two complete passes are now proven to receive 94 FHM
packages containing the `NDXR`/`MATE`/`NTXR` inventory above. The later service
slot `+0x10` consumes one or two selected packages and returns/initializes the
type-`0x98` runtime object stored at constructed-object offset `+0x15c`. This is
the first verifiable geometry-resource consumer boundary. The global service
pointer at `0x826a0728` is zero in the static image and initialized at runtime;
therefore its concrete `+0x10/+0x18/+0x1c` callees cannot yet be assigned from
the static XEX alone.

The loader-owner object's exact vtable begins at `0x82055190`; Microsoft RTTI
identifies it as `X360UnitManager`, derived from `ACE6::CAce6UnitManager`. It
contains `0x820a85e0` at slot `+0x0c`. Its virtual factories return runtime
objects whose concrete subtype is still open; those objects receive the
type-`0x98` resource at `+0x15c` and have three consecutive floats cleared at
`+0x50/+0x54/+0x58`. Calls following resource attachment use slots
`+0x130`, `+0xac`, and `+0x54`, but the recovered bodies do not yet prove draw
submission. They remain address-named. This closes the geometry boundary but
not a renderer boundary. See `ENTRY9_X360_UNIT_MANAGER_REPORT.md`.

## Native gates

- GCC ASan+UBSan: 10/10 tests passed;
- Clang ASan+UBSan: 10/10 tests passed;
- MinGW-w64 x86-64: resource-archive test and entry-9 diagnostic both compile
  as PE32+ console executables;
- no i686 target was built.

The full instruction range used for this reduction is retained in
`reports/820a7070-range.log`.
