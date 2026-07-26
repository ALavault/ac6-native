# AC6 NDXR bounded structure pass

Date: 2026-07-15 (Europe/Paris).

## Follow-up: graphics clumps and first rendered models

The former neutral boundary has now been reopened using the retail
`r_f16c`/`r_f18f` resources and the matching NUD-family descriptor layout.
The header fields are now bounded as:

- `0x08`: big-endian `0x0200` version; `0x0a`: polyset/object count;
- `0x0c/0x0e`: signed bone-index start/end;
- `0x10 + 0x30`: polygon/index clump start, followed by its size at `0x14`;
- `0x18`: vertex clump size; `0x1c`: weighted/additional vertex clump size;
- the name clump follows those three clumps; `0x20`: bounding sphere.

Object and polygon descriptors are both `0x30` bytes. The native parser reads
object names, polygon ownership, vertex formats/counts, 16-bit indices and
position vectors from either the ordinary or weighted vertex stream. This has
been executed on the two first-CUT aircraft: 9,711 positions and 14,492 indices
are joined to their Scene/MOP identities and rendered by `ac6-scene-shell`.

This follow-up supersedes the neutral field statements below for those proven
fields. It does not yet claim full-corpus support for every vertex format,
material linkage, skinning behavior or primitive/restart convention.

## Implemented native slice

The native AC6 library now has a bounds-checked, big-endian parser for every
`NDXR` resource reached by the recursive FHM walk. It validates the signature,
the exact declared payload size and the sum of three 16-byte-aligned region
lengths at offsets `0x10`, `0x14` and `0x18`. The byte at `0x0b` and words at
`0x0c` and `0x1c` remain explicitly neutral header fields.

No field is named as a mesh count, vertex buffer, index buffer, material,
joint, or string table. Those meanings are plausible follow-up hypotheses,
not results of this pass.

## Full retail gate

The updated `ac6-asset-manifest` decoded all 926 DATA entries and recursively
walked the same 56,514 FHM rows. It parsed all 2,228 `NDXR` objects without an
exception:

```text
ndxr_parsed=2228
ndxr_versions=2:2228
ndxr_region_stats=0:2228,0,321,0xb0,0x2d640;1:2228,0,524,0x10,0x110250;2:2228,0,797,0x70,0xc7ece0;3:2228,0,411,0x4f,0x24fa71
ndxr_aligned_region_sizes=2228,2228,2228
ndxr_header_words_nonzero=374,115
```

Each region tuple is `slot:count,zero_count,distinct_count,min,max`; slot 3 is
the remaining trailing byte count after the three described regions. Thus all
three lengths are nonzero and 16-byte aligned in this corpus, but all resources
also retain a nonempty trailing region. The two nominally unknown words cannot
be treated as reserved zeroes: offsets `0x0c` and `0x1c` are nonzero in 374 and
115 wrappers respectively.

The generated CSV remained byte-identical to the prior manifest:

```text
SHA-256 e77a6e897a9be68b29dbc391e24119121b9958cad5c13230ebdd580fec334cfa
manifest_byte_identical=yes
```

This is a structural validation result only. It does not prove rendering
semantics, buffer element layouts, topology, coordinate conventions, or that a
given `NDXR` is independently renderable.

## Files

- `reconstruction/ace-combat-6/include/ac6/ndxr.h`
- `reconstruction/ace-combat-6/src/ndxr.cpp`
- `reconstruction/ace-combat-6/tests/ndxr_tests.cpp`
- `reconstruction/ace-combat-6/CMakeLists.txt`
- `reconstruction/ace-combat-6/tools/asset_manifest_tool.cpp`
- `workspaces/ace-combat-6/reports/ndxr-structure-summary.txt`

## Executed commands

```bash
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6-ndxr \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .build/ace-combat-6-ndxr -j2
ctest --test-dir .build/ace-combat-6-ndxr --output-on-failure
cmake -S reconstruction/ace-combat-6 \
  -B .build/ace-combat-6-ndxr-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build .build/ace-combat-6-ndxr-sanitize -j2
ctest --test-dir .build/ace-combat-6-ndxr-sanitize --output-on-failure
.build/ace-combat-6-ndxr/ac6-asset-manifest \
  workspaces/ace-combat-6/game-files/DATA.TBL \
  workspaces/ace-combat-6/game-files/DATA00.PAC \
  workspaces/ace-combat-6/game-files/DATA01.PAC \
  > /tmp/ac6-ndxr-manifest-final.csv \
  2> workspaces/ace-combat-6/reports/ndxr-structure-summary.txt
cmp /tmp/ac6-ndxr-manifest-final.csv \
  workspaces/ace-combat-6/reports/fhm-asset-manifest.csv
```

The test suite contains five executables after this pass; all five passed in
both the ordinary and AddressSanitizer/UndefinedBehaviorSanitizer builds.
