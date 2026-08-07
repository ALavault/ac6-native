# Micro-execution snapshots

Evidence for cycles 1089 to 1092: the retail `*Bin` scenario parsers executed on
real Mission 01 bytes, compared against a native parser written from
`analysis/scenario-schema/` alone.

No console emulator, no bridge and no native product run is involved. Each
snapshot is one function, a synthetic heap and a few thousand p-code steps.

## Layout

| path | contents |
| --- | --- |
| `*.ppc.json` | Ghidra p-code micro-execution, `implementation: ppc-pcode` |
| `*.native.json` | schema-derived reimplementation, `implementation: native` |
| `*.comparison.json` | the verdict, `mode: pair`, `classification: pair_equal` |
| `objbin-batch/`, `orderbin-batch/`, `maneuver-batch/`, `actbin-batch/`, `setbin-batch/`, `tail-batch/` | one triple per node, named by payload offset |

The four files at the top level are the reference cases the reports quote; the
batch directories carry the breadth that found the sizer quirks.

**138 comparisons, 138 `pair_equal`.**

## What the values are

`memory_writes` holds guest pointers the parsers computed, laid into a synthetic
address space — `0xB0000000` for the payload, `0xB4000000` for the record,
`0xB5000000` for the sub-record buffer. No retail payload bytes are stored here.

Written bytes are detected by the union of two poison passes, `0xCD` and `0x00`,
because a parser can legitimately write a byte equal to any single poison. See
cycle 1090.

## Regenerating

```sh
PAYLOAD=reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin

# the PPC side, in the canonical project; NODES is one offset or a comma list
JAVA_TOOL_OPTIONS="-Duser.home=$PWD/ghidra-user" \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  ghidra-projects ace-combat-6 -process default.xex -scriptPath scripts \
  -postScript MicroExecuteScenarioParser.java 0x82330158 "$NODES" "$PWD/$PAYLOAD" \
    "$PWD/analysis/microexec/objbin-read.ppc.json" ObjBin \
  -readOnly -noanalysis

# the native side
python3 tools/emit_ac6_native_snapshot.py ObjBin 0xfa0 "$PAYLOAD" \
  --output analysis/microexec/objbin-read.native.json

# the verdict
python3 tools/compare_ac6_function_snapshots.py \
  --ppc analysis/microexec/objbin-read.ppc.json \
  --native analysis/microexec/objbin-read.native.json \
  --output analysis/microexec/objbin-read.comparison.json
```

The payload is a decoded retail extract and is not committed. Regenerating it is
covered by `docs/STATIC_EVIDENCE_TOOLING.md`.

## Scope

These snapshots prove the parsers' **structure** is reproduced exactly. They say
nothing about semantics: the parsers copy pointers without interpreting them and
the native parser does the same, so their agreement is evidence about layout, not
about meaning.
