# AC6 NDXR record contract at 0x822c2148

Date: 2026-07-15 (Europe/Paris).

## XEX evidence

The existing decompilation and a fresh 83-instruction assembly dump establish
the following bounded contract for XEX function `0x822c2148`:

- accept a direct `NDXR`/`NDP3` pointer, or skip a 16-byte `GIDX` prefix;
- require header byte `+0x08 == 2` and byte `+0x09 == 0`;
- address record `i` at `(i + 1) * 0x30` from the resource signature;
- reject the record if big-endian halfword `record+0x26` has bit `0x4` set;
- otherwise copy the four PowerPC floats at record offsets `0`, `4`, `8` and
  `0xc` to a three-float output plus a separate float output.

Evidence files:

- `workspaces/ace-combat-6/exports/822c2148.json`;
- `workspaces/ace-combat-6/reports/822c2148.asm`;
- `workspaces/ace-combat-6/reports/re-agent-822c2148-dry-run.log`.

The re-agent invocation used `--dry-run --skip-parity`; it reported
`Dry run mode — no LLM calls will be made` and `Would reverse: 0x822c2148`.

The export has no recovered caller, so this pass does not name the four values
as position, extent, radius, bounding box or another rendering concept.

## Native implementation

`NdxrFunction822c2148Record` reproduces the four big-endian float loads, flag
load and exact bit gate. `ndxr_function_822c2148_record_capacity` supplies a
fail-closed native capacity: only complete `0x30` records between offset `0x30`
and the already validated first NDXR region boundary are addressable. This
capacity is a safety bound, not a claim that every slot is used by the game.

Malformed signature/size/region bounds, unsupported version/byte `0x09`, and
out-of-capacity indices throw before any record access.

## Full retail gate

The complete 926-entry, 56,514-row FHM traversal produced:

```text
ndxr_parsed=2228
ndxr_function_822c2148=250766,248325,201,272,2,3872
```

The tuple is:

```text
bounded_record_slots,accepted_by_bit_gate,nonfinite_float_values,
distinct_capacities,min_capacity,max_capacity
```

Thus 250,766 bounded slots were decoded; 248,325 pass the XEX bit test and
2,441 do not. The 201 non-finite values are preserved exactly. They disprove a
tempting blanket invariant that all four words are finite geometry values;
the parser neither rejects nor normalizes them.

The manifest CSV stayed byte-identical to the previous inventory:

```text
SHA-256 e77a6e897a9be68b29dbc391e24119121b9958cad5c13230ebdd580fec334cfa
manifest_byte_identical=yes
```

Aggregate evidence is in
`workspaces/ace-combat-6/reports/ndxr-function-822c2148-summary.txt`.

## Validation

Linux tests:

```bash
cmake --build .build/ace-combat-6-ndxr -j2
ctest --test-dir .build/ace-combat-6-ndxr --output-on-failure
```

Result: 5/5 passed.

AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
cmake --build .build/ace-combat-6-ndxr-sanitize -j2
ctest --test-dir .build/ace-combat-6-ndxr-sanitize --output-on-failure
```

Result: 5/5 passed.

Windows cross-compilation, performed directly because the installed MinGW
sysroots do not provide the zlib dependency required by the complete archive
tool:

```bash
x86_64-w64-mingw32-g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion \
  -Ireconstruction/ace-combat-6/include \
  reconstruction/ace-combat-6/src/ndxr.cpp \
  reconstruction/ace-combat-6/tests/ndxr_tests.cpp \
  -o /tmp/ac6-ndxr-tests-x64.exe
i686-w64-mingw32-g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion \
  -Ireconstruction/ace-combat-6/include \
  reconstruction/ace-combat-6/src/ndxr.cpp \
  reconstruction/ace-combat-6/tests/ndxr_tests.cpp \
  -o /tmp/ac6-ndxr-tests-x86.exe
```

Both builds succeeded. `file` identifies the outputs as PE32+ x86-64 and PE32
i386 respectively. They were compiled but not executed under Windows in this
pass.
