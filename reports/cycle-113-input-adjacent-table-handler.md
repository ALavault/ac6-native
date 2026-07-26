# AC6 cycle 113 — adjacent input-table handler boundary

Date: 2026-07-17

## Scope

Read-only, headless inspection of the qualified Xbox 360 XEX project. This
cycle follows the input aggregate contract at `0x82215418` and inspects the
next entry in the same data table, without assigning gameplay names or
modifying generated code.

Target identity remains the AC6 Xbox 360 `default.xex` target recorded by the
workspace manifest and prior cycles. The Ghidra project used was
`ghidra-projects/ace-combat-6-corrected`, opened with `-readOnly -noanalysis`.

## Evidence

The table at `0x82080c40` is a sequence of handler/metadata pairs. The
relevant entries are:

```text
0x82080c78  0x82215418  0x40002d03
0x82080c80  0x822154d0  0x40003f02
0x82080c88  0x822155d0  0x40002005
```

The handler at `0x822154d0` is reached as an interior branch target from the
contiguous body beginning at `0x822154b0`. Its bounded assembly shows:

- a conditional branch on the incoming `cr6` state;
- calls to `0x8233d120`, `0x8233d150`, `0x8233d280` and `0x8233d1c0`;
- a floating-point conversion and division using global locations
  `0x82001348`, `0x826ede30`, `0x826ede18` and `0x826ede20`;
- writes of zero-like values to an object held in `r29` at offsets `+0x08` and
  `+0x10`.

The only direct call found to the containing entry `0x822154b0` is the
constructor-like call at `0x822157c0`. No direct call to `0x822154d0` was
found, which is consistent with the table being runtime/data-driven rather
than a statically enumerated call graph.

The decompiler output for `0x822154d0` is not an ordinary standalone C++
method: it depends on the incoming condition-register state and on register
values established by the preceding body. Therefore the apparent globals and
zeroing stores are recorded as raw ABI evidence only.

## Decision

Classify this table slot as `runtime_table_handler_needs_abi_evidence`.

Do not:

- call it an input consumer;
- assign a gameplay meaning to metadata `0x40003f02`;
- infer a field type from the `r29+0x08/+0x10` writes;
- add a native wrapper or modify generated output;
- claim that the flight/input chain is statically closed.

The existing `0x82215418` aggregate contract remains valid. The next useful
join is a runtime observation of the table dispatch and condition-register
precondition, deferred until a human-authorized Xenia/XenonTests session is
requested. This is a dynamic evidence boundary, not a request for action in
the current autonomous pass.

## Commands

```bash
HOME=/tmp/ac6-ghidra-cycle113-home \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6-corrected \
  -process default.xex -readOnly -noanalysis \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x822154b0 0x82215620 \
  -postScript DecompileAt.java 0x822154d0 \
  -postScript ReferencesTo.java 0x822154d0

HOME=/tmp/ac6-ghidra-cycle113-home3 \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6-corrected \
  -process default.xex -readOnly -noanalysis \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpU32Range.java 0x82080c20 0x82080cc0 \
  -postScript FindDirectCallsTo.java 0x822154d0
```

The complete bounded logs are kept outside the repository in
`/tmp/ac6-cycle113-input-adjacent.log` and
`/tmp/ac6-cycle113-table.log`.
