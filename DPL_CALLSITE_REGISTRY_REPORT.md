# AC6 DPL call-site and physical-index report

Date: 2026-07-15

## Exhaustive direct-call recovery

Decoding every aligned PowerPC branch-and-link instruction in the XEX recovers
20 direct calls to `0x821d1060`; Ghidra's exported xrefs reported only one:

```text
82097914 820979c0 820979dc 82097b50
820a8634 820a87f0 820a8d04
8215a32c
8218c970 8218c9f4 8218cbd8 8218cc5c
8219c18c 8219c238 8219c254 8219c3c8
8219fc74 8219fd20 8219fd3c 8219feb0
```

They form five code-owner clusters. The `0x820979xx` cluster consumes several
ids returned by `0x821b7300`, resolves DPL nodes, and selects numbered children
from the returned node. The `0x820a86xx` cluster includes the already proven
campaign selector path. The other three clusters are additional gameplay/UI
resource consumers; none is the archive-table constructor.

`0x821b7300` is not a physical archive-index producer. It first classifies its
input through `0x821b7430`, then emits mode-specific resource identifiers from
XEX tables at `0x820658d8`, `0x82065920`, and `0x820659a0`, with special values
such as `0x87`, `0x92`, `0x96`, and `0xa1`.

## Refutation boundary

The hypothesis "DPL resource id is universally the physical DATA.TBL index"
is refuted. `0x821b6e58` produces valid DPL ids from `0x75e` upward in modes 2,
4, and 5, while the supplied physical table has only 926 entries (indices
0..925, maximum `0x39d`). These namespaces therefore cannot be identical.

The newly recovered request state machine resolves the low-id subset exactly:
`0x821d1128` queues every DPL id below `0x39d` unchanged as a DATA.TBL index.
The namespaces remain non-identical in general because ids at and above that
boundary take a separate route, but DPL id 9 is now proven to select physical
entry 9. The DPL hash need not occur inside the payload: it is the registry key
used before the numeric archive request is created.

## DATA.TBL loader result

`0x821cc250` reads `sim:DATA.TBL`, stores `buffer+8` at global `0x8293ba38`,
the entry count at `0x8293ba30`, and establishes related bounds.
`0x821cc4d0` is an asynchronous
read/state machine. Its downstream blocks at `0x821ccdb4`, `0x821cce60`, and
`0x821ccf6c` consume the table base, but operate on internal halfword tables and
`0x44`-byte runtime records; none formats a DPL name or calls `0x821d1060`.

## Executable diagnostic

`ac6-mission-diagnostic` is a deterministic native shell diagnostic. For
selector 1 it now emits the proven DPL id, spelling, hash, and physical entry 9
with status `direct_data_table_route_proven`.

Validation:

- GCC ASan+UBSan: 10/10 tests;
- Clang ASan+UBSan: 10/10 tests;
- MinGW x64: PE32+ executable compiled;
- re-agent/Codex on `0x821b6e58`: PASS, objective PASS.

## Next concrete lock

Promote the already inventoried entry-9 payload family into the deterministic
campaign asset allowlist, then identify which child-1 subrecords feed the scene
and renderer consumers. See `DPL_ARCHIVE_HANDLE_CHAIN.md` for the exact join.
