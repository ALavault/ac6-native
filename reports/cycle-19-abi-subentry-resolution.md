# Cycle 19 — AC6 PPC ABI subentry resolution

## Scope

Read-only static evidence for the active PAL `default.xex` project, qualified
by SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
No Ghidra function boundary, symbol, noreturn attribute, generated
XenonRecomp output, or XEX data was modified.

## Reproduced evidence

`Function_821D0C58` directly calls `0x82382efc`. The read-only
`VerifyPpcAbiSaveRestoreHelpers.java` run records:

```text
0x82382efc: std r29,-0x20(r1)
0x82382f00: std r30,-0x18(r1)
0x82382f04: std r31,-0x10(r1)
0x82382f08: stw r12,0x8(r1)
0x82382f0c: blr
```

The surrounding island saves `r14..r31` and LR, then restores them at
`0x82382f10..0x82382f60`. Thus `0x82382efc` is the `r29` callee-save subentry
of an ABI helper. The project splitting it into short noreturn functions is a
Ghidra boundary/metadata artifact.

## Decision

`0x821d0c58` is statically understood as an ABI helper call, not a task
service, game-system boundary, or XenonRecomp candidate. The apparent
continuation in `0x821d0cf8` is compatible with the helper's return. No title
or gameplay semantics are asserted.

## Reproduction

```sh
HOME=/tmp/ac6-ghidra-cycle19-home \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -scriptPath workspaces/ace-combat-6/scripts \
  -postScript VerifyPpcAbiSaveRestoreHelpers.java -noanalysis \
  | tee workspaces/ace-combat-6/reports/ghidra-cycle-19-abi-helper.log
```

Next: choose a game-owned caller after its ABI prologue and obtain independent
static or Xenia/XenonTests evidence before assigning task-system semantics.
