# DATA.TBL entry 9 Scene resource resolution

Date: 2026-07-15

## Exact archive join

The 553 fixed-record paths previously recovered below top-level entry-9
children 22 and 23 now resolve without hashing or string search. Every Scene
group has the same immediate FHM sibling layout:

- member `N`: an `NFIC` payload whose first eight bytes are `NFICCUT\0`;
- member `N+1`: an FHM with one member per Scene path;
- member `N+2`: the flat `0x80`-record Scene path table.

Path record index `i` names resource-FHM member index `i`. All 44 Scene groups
have matching cardinality, and all 553 path records resolve to bounded payloads.
The native `resolve_scene_resource` function implements only this exact index
join. It rejects mismatched cardinality, invalid indices, and members extending
beyond the paired FHM span.

The deterministic artifact is
`reports/entry9-scene-path-resolution.csv`. It has 554 lines including the
header, is 79,791 bytes, and has SHA-256
`89916118378798ea3865f572c378e39027aab9fa8f1a9237e6953c3f451cde79`.
A second generation compared byte-identical.

## First mission resource

The first recovered path resolves as follows:

- path: `Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop`;
- Scene table: FHM path `22.1.0.2`, record 0;
- resource: FHM path `22.1.0.1.0`, entry-9 absolute offset 36,953,728,
  payload size 4,656 bytes;
- adjacent state payload: FHM path `22.1.0.0`, entry-9 absolute offset
  36,914,048, size 39,352 bytes, prefix `NFICCUT\0`.

The strings `Tcam` and `.mop` remain retail path evidence rather than assigned
runtime semantics.

## Bounded GYZ wrapper

All 553 resolved resources satisfy the same structural `GYZ` wrapper contract:

- a big-endian declared size at payload offset `0x20`;
- a big-endian data offset at payload offset `0x30` (observed as `0x40` in
  these resources);
- `GYZ\0` at the data offset;
- an inner declared size at data offset `+0x08` equal to the outer declared
  size;
- an inner header size at data offset `+0x0c` bounded by the declared size;
- the declared size equals the remaining payload after the data offset.

For the first resource, the data offset is 64 and the declared GYZ size is
4,592 bytes. Across the complete set, resource payload sizes range from 288 to
23,072 bytes and total 2,768,096 bytes. `MopGyzView` exposes only the proven
offset and size boundaries; the GYZ contents are not yet semantically named.

## Runtime boundary

The XEX state path at `0x8209b6b0` calls the address-based routine
`0x8209b734`; when the preceding result equals `0x0f` and the object flag at
`+0x1e0` is set, it calls `0x820aa670` with mode 2. Routine `0x820aa670` makes
two count/element passes through helpers `0x82291fa8` and `0x82291fb8` and uses
type-`0x98` service slots during runtime object construction.

This is a verifiable campaign-state gate near the already proven entry-9
consumer, but it is not yet an exact executable join from raw child 22/23 data.
The local view initialized by `0x82291f88` is an offset-table container, not
the flat Scene path table. The archive adjacency to `NFIC CUT` is therefore
proven data structure, while mission activation and Scene traversal remain
open. No renderer-oriented or speculative function name is introduced.

## Validation

- GCC ASan+UBSan: 12/12 tests passed;
- Clang ASan+UBSan: 12/12 tests passed;
- MinGW-w64 x86-64: Scene tests, GYZ tests, and the entry-9 diagnostic compile
  as PE32+ console executables;
- malformed lookup and GYZ size/magic/offset cases are covered;
- no i686 target was built.

No re-agent generation was used. The remaining executable frontier is not an
exact joined leaf, so generating semantic code there would exceed the current
evidence.
