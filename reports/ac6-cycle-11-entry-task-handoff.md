# AC6 cycle 11 — entry handoff to a synchronized task loop

## Qualified scope

- target: `ace-combat-6-pal`
- XEX SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- entry call: `0x821f6024 -> 0x821d7d90`
- method: bounded Ghidra `-noanalysis` reads and mnemonic assertion

## Static result

After command-line tokenization, the entry wrapper calls `0x821d7d90`.
Raw instructions show that this routine calls `0x821d5ef8` and `0x821d6bd0`,
performs repeated synchronization/import calls, invokes `0x821d7a90` and
`0x821d7c80`, dispatches two virtual methods through global objects, then
branches back from `0x821d7e88` to `0x821d7e34`.

This is a persistent synchronized task/loop handoff. It is the first
entry-adjacent nontrivial execution region after platform lifecycle setup, but
static evidence does not establish that it is the game loop, title loop, or a
resource loader. The next useful evidence is a bounded dynamic observation of
the two initializers or the virtual dispatches.

## Reproduction

```sh
JAVA_TOOL_OPTIONS='-Duser.home=/tmp/ac6-ghidra-home' \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript VerifyEntryTaskHandoff.java -noanalysis
```
