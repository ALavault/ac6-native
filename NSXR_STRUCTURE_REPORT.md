# AC6 NSXR bounded structure pass

Date: 2026-07-15 (Europe/Paris).

## Native slice

The native library now parses the bounded big-endian structure of `NSXR`
graphics-resource wrappers. It validates:

- the `NSXR` signature and exact declared payload size;
- neutral header byte fields and words through offset `0x24`;
- five 16-byte-aligned, monotonic, in-payload region offsets at
  `0x28..0x38`;
- safe region slicing, with the payload size as the end of region 4.

The fields and regions are deliberately unnamed. This pass does not claim that
they are shader stages, Xbox microcode, reflection data, constants, or lookup
tables.

## Fail-closed correction

The first corpus attempt placed the offset array four bytes too late. It stopped
immediately at `entry 163/path 0` with `NSXR region offsets are not monotonic`.
Re-reading the bytes established two neutral words at `0x20/0x24`, followed by
the five offsets at `0x28..0x38`. The parser and synthetic vectors were
corrected before any result was accepted.

## Retail corpus gate

The complete 926-entry, 56,514-row recursive traversal passed all 51 `NSXR`
resources:

```text
nsxr_parsed=51
nsxr_versions=2:51
nsxr_region_offset_stats=0:51,1,0x60,0x60;1:51,1,0x70,0x70;2:51,1,0x80,0x80;3:51,1,0x90,0x90;4:51,26,0x3e0,0x18e0
nsxr_strictly_increasing_offsets=51
```

Each offset tuple is `slot:count,distinct,min,max`. The first four boundaries
are constant in this corpus; the fifth has 26 values. This is a population
fact, not permission to hard-code resource meanings.

The generated manifest remained byte-identical:

```text
SHA-256 e77a6e897a9be68b29dbc391e24119121b9958cad5c13230ebdd580fec334cfa
manifest_byte_identical=yes
```

Aggregate evidence is stored in
`workspaces/ace-combat-6/reports/nsxr-structure-summary.txt`.

## Files

- `reconstruction/ace-combat-6/include/ac6/nsxr.h`
- `reconstruction/ace-combat-6/src/nsxr.cpp`
- `reconstruction/ace-combat-6/tests/nsxr_tests.cpp`
- `reconstruction/ace-combat-6/CMakeLists.txt`
- `reconstruction/ace-combat-6/tools/asset_manifest_tool.cpp`

## Validation

```bash
cmake --build .build/ace-combat-6-ndxr -j2
ctest --test-dir .build/ace-combat-6-ndxr --output-on-failure
cmake --build .build/ace-combat-6-ndxr-sanitize -j2
ctest --test-dir .build/ace-combat-6-ndxr-sanitize --output-on-failure
```

Both Linux gates passed 6/6 tests. The sanitizer build enables AddressSanitizer
and UndefinedBehaviorSanitizer.

The isolated parser and its test also compile with both installed MinGW
targets:

```bash
x86_64-w64-mingw32-g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion \
  -Ireconstruction/ace-combat-6/include \
  reconstruction/ace-combat-6/src/nsxr.cpp \
  reconstruction/ace-combat-6/tests/nsxr_tests.cpp \
  -o /tmp/ac6-nsxr-tests-x64.exe
i686-w64-mingw32-g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion \
  -Ireconstruction/ace-combat-6/include \
  reconstruction/ace-combat-6/src/nsxr.cpp \
  reconstruction/ace-combat-6/tests/nsxr_tests.cpp \
  -o /tmp/ac6-nsxr-tests-x86.exe
```

The results are PE32+ x86-64 and PE32 i386 executables. They were not executed
under Windows in this pass.
