# AC6 cycle 114 — adjacent table lifecycle sweep

Date: 2026-07-17

## Scope

This is a read-only, headless sweep of the handler pairs immediately following
the input aggregate entry in the Xbox 360 `default.xex` table at
`0x82080c40`. It tests whether the adjacent entries provide a statically
qualified keyboard, flight or aircraft consumer. No Xenia, Wine, GUI or human
session was started.

## Table slice

```text
entry       handler       metadata
0x82080c80  0x822154d0    0x40003f02
0x82080c88  0x822155d0    0x40002005
0x82080c90  0x82215650    0x40003204
0x82080c98  0x82215718    0x40003504
0x82080ca0  0x822157f0    0x4000ee05
0x82080ca8  0x82215ba8    0x40000a03
0x82080cb0  0x82215be8    0x40001804
0x82080cb8  0x82215c80    0x40001d03
0x82080cc0  0x82215cf8    0x40002d06
0x82080cc8  0x82215db0    0x40001304
0x82080ccc  0x82215e38    0x40002d06
0x82080cd8  0x82215ef0    0x40001304
0x82080ce0  0x82215f70    0x40001d03
0x82080ce8  0x82215fe8    0x40002d06
0x82080cf0  0x822160a0    0x40001304
0x82080cf8  0x82216120    0x40003304
0x82080d00  0x822161f0    0x40001804
0x82080d08  0x82216280    0x40001804
```

## Qualified contracts

- `0x822155d0` converts a runtime counter from `0x8233d188` using the globals
  at `0x826ede18`, `0x826ede28` and `0x826ede30`; it is a time/scaling helper.
- `0x82215650` scales an incoming stack float by `DAT_82005e9c`.
- `0x82215718` combines the runtime counter with a 64-bit field at
  `param+0x08` relative to `DAT_826ede18`.
- `0x822157f0`, `0x82215ef0` and `0x82215f70` are destructor/deallocation
  wrappers with vtable assignment or `Function_82383670` ownership behavior.
- `0x82215ba8` ORs `0x200` into `object+0x34` and raises `object+0x1160` to
  at least `10`.
- `0x82215be8` walks 64 pointers from `object+0x2448c`, invokes each non-null
  first virtual slot, then clears the pointer.
- `0x82215c80` and `0x82215fe8` are empty functions.
- `0x82215cf8` resets a structure: fields around `+0x4084..+0x40b8` are
  initialized and 64 byte slots at `object+4 + index*0x100` are cleared.
- `0x82215db0` and `0x82215e38` are flag predicates. They inspect bits at
  `object+0x118`, `+0x124`, `+0x130`, an indirect object reached through
  `object+0x180`, and a nested byte at `+0x55`; neither writes flight/input
  state.
- `0x822160a0` is an indirect callback/jump-table dispatch whose target depends
  on incoming registers and `cr6`.
- `0x82216120` writes caller-supplied values to an object offset supplied in a
  register and sets an additional register-derived global flag; the missing
  register provenance prevents an absolute address or stronger field name.
- `0x822161f0` and `0x82216280` are construction/initialization paths. The
  latter installs many vtable pointers and subobjects at offsets such as
  `+0x29c84`, `+0x2d3b8`, `+0x31080`, `+0x36050` and `+0x362ac`.

## Decision

This table slice is a runtime lifecycle/time/object-service cluster, not a
statically qualified keyboard or flight consumer. The apparent flags and
callback entries are deliberately kept offset- and ABI-qualified; no input
bit, aircraft state, camera state or mission semantic is assigned.

The AC6 input contract from cycle 112 and the dynamic boundary from cycle 113
remain unchanged. The next proof of a flight consumer still requires a runtime
observation of the data-driven dispatch or an objective-bearing first-mission
trace. That is deferred until the user explicitly requests a human-authorized
Xenia/XenonTests session; it is not a blocker for autonomous static work.

## Evidence commands

```bash
HOME=/tmp/ac6-ghidra-cycle114-home \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6-corrected \
  -process default.xex -readOnly -noanalysis \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpU32Range.java 0x82080c80 0x82080d10 \
  -postScript DecompileAt.java 0x822155d0 \
  -postScript DecompileAt.java 0x82215650 \
  -postScript DecompileAt.java 0x82215718 \
  -postScript DecompileAt.java 0x822157f0 \
  -postScript DecompileAt.java 0x82215ba8

HOME=/tmp/ac6-ghidra-cycle114-home2 \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6-corrected \
  -process default.xex -readOnly -noanalysis \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DecompileAt.java 0x82215be8 \
  -postScript DecompileAt.java 0x82215c80 \
  -postScript DecompileAt.java 0x82215cf8 \
  -postScript DecompileAt.java 0x82215db0 \
  -postScript DecompileAt.java 0x82215e38 \
  -postScript DecompileAt.java 0x82215ef0 \
  -postScript DecompileAt.java 0x82215f70 \
  -postScript DecompileAt.java 0x82215fe8 \
  -postScript DecompileAt.java 0x822160a0 \
  -postScript DecompileAt.java 0x82216120 \
  -postScript DecompileAt.java 0x822161f0 \
  -postScript DecompileAt.java 0x82216280
```

Logs: `/tmp/ac6-cycle114-table-handlers.log` and
`/tmp/ac6-cycle114-table-handlers2.log`.
