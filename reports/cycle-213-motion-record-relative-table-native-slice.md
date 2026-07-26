# AC6 cycle 213 — native bounded slice for type `0x8181` motion records

## Target and evidence

- target: `ac6-xbox360-pal`
- module: `default.xex`
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- retail functions: `0x82118a50` and `0x8211bd50`
- evidence exports: `exports/82118a50.json`, `exports/8211bd50.json`
- preceding static boundary: `reports/cycle-212-motion-record-producer-boundary.md`

## Reconstructed boundary

The native library now contains a bounded, guest-big-endian implementation of
the directly observable type-`0x8181` portion of `0x82118a50`:

1. read the tag at `record+0x0a` and reject every tag except `0x8181`;
2. read the count at `+0x14` and the materialized bit at `+0x18`;
3. when the bit is clear, materialize `record+0x1c` from the relative word at
   `+0x10`;
4. materialize each `0x20`-stride entry's `+0x10` word relative to the record
   guest address;
5. set bit `0x80000000` at `record+0x18`;
6. expose the bounded type-`0x8181` lookup from `0x8211bd50` only after that
   bit is present.

The implementation lives in:

- `reconstruction/ace-combat-6/include/ac6/motion_record.h`
- `reconstruction/ace-combat-6/src/motion_record.cpp`
- `reconstruction/ace-combat-6/tests/motion_record_tests.cpp`

It uses an explicit 32-bit guest base and bounded mutable byte span. This keeps
guest pointers separate from host pointers and rejects missing ranges instead
of dereferencing arbitrary memory.

## Explicitly outside this slice

Retail `0x82118a50` invokes `0x82119740` once per newly materialized entry.
Its side effects and ABI remain unreconstructed, so the native result reports
`entry_normalizer_still_required`; it is not presented as a complete
replacement for the retail function.

The type-`0x0011` branch delegates to opaque `0x82339718`, and `0x8211bd50`
delegates that type to `0x82339508`. Those branches remain outside this native
slice. The work does not identify an aircraft, camera, mission, post-CUT
receiver, or gameplay meaning for the records.

## Validation

```text
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6/native
cmake --build .build/ace-combat-6/native -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure -R '^ac6-motion-record-tests$'
clang-format --dry-run --Werror reconstruction/ace-combat-6/include/ac6/motion_record.h reconstruction/ace-combat-6/src/motion_record.cpp reconstruction/ace-combat-6/tests/motion_record_tests.cpp
cmake --build .build/ace-combat-6/native -j16
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

The dedicated test passes. The complete AC6 corpus passes **42/42** after the
addition. It covers type rejection, two-entry materialization, materialized
repeat behavior, bounded lookup, invalid index, and truncated input.

No GUI, Xenia run, generated XenonRecomp edit, retail asset copy, or human
interaction was used.
