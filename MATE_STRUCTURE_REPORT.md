# AC6 MATE bounded structure pass

Date: 2026-07-15 (Europe/Paris).

## Scope and evidence boundary

`MATE` is the next graphics-adjacent format after the NDXR and NSXR wrappers.
The current export set contains no decompiled function with a `MATE` string
reference, so this pass is corpus-grounded rather than assigned an unproven XEX
consumer. No LLM or Ollama service was used.

The native parser validates only directly observable structure:

- the `MATE` signature;
- four neutral big-endian header halfwords at `0x04..0x0a`;
- three aligned, monotonic, in-payload region offsets at `0x0c..0x14`;
- safe spans for the three regions, using payload end for region 2.

It does not identify materials, textures, shader parameters, hashes, flags or
string tables. Visible strings such as `NU_HASH` in some records remain bytes,
not schema claims.

## Exact first-world model binding follow-up

The first joined aircraft provides a stronger, model-local MATE variant. Its
seven region-0 entries are `0x10`-stride absolute material offsets. Its 54
region-1 entries are `0x10`-stride batch bindings: high 16 bits are sequential
batch ordinals `0..53`, low 16 bits select material `0..6`, and all reserved
words are zero. The batch count exactly equals the paired NDXR polygon count.
Material texture count is at `+0x0a`; texture identifiers start at `+0x20` with
stride `0x18`. These records match the NDXR polygon texture records byte for
byte. The same structural checks close the second joined aircraft.

This is deliberately a bounded model variant, not a promotion of those field
meanings to all 733 MATE wrappers. The native parser rejects table overflow,
non-sequential batch ordinals, out-of-range material indices and nonzero
reserved table words. A synthetic batch-to-material-to-texture case is covered
by `ac6-mate-tests`.

## Full retail gate

All 733 signature-classified wrappers passed in the complete archive walk:

```text
mate_parsed=733
mate_region_offset_stats=0:733,1,0x30,0x30;1:733,18,0x40,0x160;2:733,66,0x50,0x1450
mate_header_halfword_stats=0:733,18,0x1,0x13;1:733,62,0x1,0x139;2:733,56,0x1,0xe3;3:733,1,0x0,0x0
```

Tuples are `slot:count,distinct,min,max`. The first boundary is always `0x30`
in this corpus, while boundaries 1 and 2 have 18 and 66 values. Therefore the
common `0x30/0x40/0x50` example is not hard-coded.

The 926-entry, 56,514-row manifest stayed byte-identical:

```text
SHA-256 e77a6e897a9be68b29dbc391e24119121b9958cad5c13230ebdd580fec334cfa
manifest_byte_identical=yes
```

Aggregate evidence:
`workspaces/ace-combat-6/reports/mate-structure-summary.txt`.

## Files and validation

- `reconstruction/ace-combat-6/include/ac6/mate.h`
- `reconstruction/ace-combat-6/src/mate.cpp`
- `reconstruction/ace-combat-6/tests/mate_tests.cpp`
- `reconstruction/ace-combat-6/CMakeLists.txt`
- `reconstruction/ace-combat-6/tools/asset_manifest_tool.cpp`

Linux and sanitizer builds both pass 7/7 tests:

```bash
cmake --build .build/ace-combat-6-ndxr -j2
ctest --test-dir .build/ace-combat-6-ndxr --output-on-failure
cmake --build .build/ace-combat-6-ndxr-sanitize -j2
ctest --test-dir .build/ace-combat-6-ndxr-sanitize --output-on-failure
```

The isolated parser and tests compile for both Windows targets:

```bash
x86_64-w64-mingw32-g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion \
  -Ireconstruction/ace-combat-6/include \
  reconstruction/ace-combat-6/src/mate.cpp \
  reconstruction/ace-combat-6/tests/mate_tests.cpp \
  -o /tmp/ac6-mate-tests-x64.exe
i686-w64-mingw32-g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion \
  -Ireconstruction/ace-combat-6/include \
  reconstruction/ace-combat-6/src/mate.cpp \
  reconstruction/ace-combat-6/tests/mate_tests.cpp \
  -o /tmp/ac6-mate-tests-x86.exe
```

The outputs are PE32+ x86-64 and PE32 i386. They were not run on Windows.
