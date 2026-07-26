# AC6 cycle 8 — entry ABI helper and preflight guard

## Qualified scope

- target: `ace-combat-6-pal`
- input: `game-files/default.xex`
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base: `0x82000000`
- method: bounded Ghidra `-noanalysis` reads only

## Verified entry slice

`0x821f5e90` first copies LR to `r12`, then calls `0x82382ef8`. The latter is
not an import, fatal path, or game-owned boot routine. It is a valid subentry
of a PPC ABI nonvolatile-register save helper:

- `0x82382ec0..0x82382ef8`: sequential `std` stores for `r14..r28`;
- entering at `0x82382ef8` saves `r28..r31` and LR, ending with `blr` at
  `0x82382f0c`;
- `0x82382f10..0x82382f60` is the matching restore helper.

Thus the instructions after the `bl 0x82382ef8` in the entry wrapper are
reachable. The first subsequent direct call is `0x821f7de8`.

Ghidra decompilation of `0x821f7de8` shows a bounded preflight guard: it calls
`0x821f7d10`; on a zero result it optionally invokes a member through imported
`KeDebugMonitorData`, then calls imported `HalReturnToFirmware(1)`. It returns
otherwise. This is boot-adjacent preflight, not yet a recovered game loop,
asset loader, or mod hook.

## Reproduction

```sh
JAVA_TOOL_OPTIONS='-Duser.home=/tmp/ac6-ghidra-home' \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript VerifyPpcAbiSaveRestoreHelpers.java -noanalysis

JAVA_TOOL_OPTIONS='-Duser.home=/tmp/ac6-ghidra-home' \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DecompileAt.java 0x821f7de8 -noanalysis
```

## Next boundary

Trace `0x821f7d10` with Xenia/XenonTests before assigning an initialization
subsystem name. Preserve the helper and the runtime imports as external ABI
boundaries; do not add them to native reconstruction output.
