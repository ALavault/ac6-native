# AC6 record-key search at 0x822c31e8

Date: 2026-07-15 (Europe/Paris).

## XEX contract

Function `0x822c31e8` is a 22-instruction PowerPC leaf with 11 recovered call
references. It receives a runtime container whose count is at `+0x08` and
record pointer at `+0x0c`, rejects a negative or out-of-range start index, then
searches signed big-endian halfword `record+0x24` with stride `0x30`. It returns
the first matching index or `-1`.

This layout is compatible with the `0x30` records already consumed at
`record+0x26` by `0x822c2148`. Compatibility is not proof that the runtime
container points directly at the serialized NDXR first region.

Evidence:

- `workspaces/ace-combat-6/exports/822c31e8.json`;
- `workspaces/ace-combat-6/reports/822c31e8.asm`.

Recovered callers request keys `2`, `4`, `9`, `10`, `11`, `0x19..0x1b`,
`0x41`, `0x42`, plus ranges based at `0x312`, `0x31e`, `0x32a` and `0x336`.
This pass does not assign meanings to those keys.

## re-agent/Codex result

One non-dry re-agent round ran with the AC6 configuration's `codex` provider
and model `gpt-5.4`; no Ollama process or provider was used. Its response
independently produced the same bounded `find_ndxr_function_822c31e8_key`
implementation: parse the wrapper, derive the existing safe capacity, reject
an out-of-capacity start, compare big-endian signed key `+0x24` at stride
`0x30`, and return an optional index.

The full prompt and response are retained at
`workspaces/ace-combat-6/reports/logs/round1-20260715-070618-reverser.json`.

## Native adaptation and corpus boundary

The portable primitive searches only complete record-shaped slots inside the
already validated first NDXR region. It cannot overrun the payload and models
XEX `-1` as `std::nullopt`.

Across all 2,228 NDXR wrappers:

```text
ndxr_function_822c31e8_keys=250766,240886,89,0x0,0xffff,1752
ndxr_function_822c31e8_caller_key_counts=0x2:1653;0x4:99
```

The first tuple is
`slots,zero_keys,distinct_unsigned_bit_patterns,min,max,fixed_caller_key_slots`.
Only fixed caller keys 2 and 4 occur in the bounded serialized population;
the other eight profiled fixed keys do not. This negative result prevents a
claim that the serialized region is already the complete runtime container.
Relocation, filtering or construction may occur elsewhere.

The full manifest remains byte-identical:

```text
SHA-256 e77a6e897a9be68b29dbc391e24119121b9958cad5c13230ebdd580fec334cfa
manifest_byte_identical=yes
```

Aggregate evidence is in
`workspaces/ace-combat-6/reports/ndxr-function-822c31e8-summary.txt`.

## Validation

The Linux and ASan/UBSan suites both pass 7/7 tests. The NDXR primitive and
tests also compile with `x86_64-w64-mingw32-g++` and
`i686-w64-mingw32-g++`; `file` identifies PE32+ x86-64 and PE32 i386 outputs.
The Windows binaries were not executed.

Modified native files:

- `reconstruction/ace-combat-6/include/ac6/ndxr.h`;
- `reconstruction/ace-combat-6/src/ndxr.cpp`;
- `reconstruction/ace-combat-6/tests/ndxr_tests.cpp`;
- `reconstruction/ace-combat-6/tools/asset_manifest_tool.cpp`.
