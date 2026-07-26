# AC6 cycle 10 — entry lifecycle platform slice

## Qualified scope

- target: `ace-combat-6-pal`
- XEX SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- entry continuation: `0x821f5ec8..0x821f5eec`
- method: bounded Ghidra `-noanalysis` reads and mnemonic assertion

## Verified sequence

After the resolved XEX preflight, the entry wrapper:

1. calls `0x821f7c40(1)`, a critical-section-protected dispatch over a global
   linked callback list;
2. calls `0x821f5ca8`, a platform policy gate using executable privilege,
   AV pack, XConfig and language imports to select bounded values;
3. calls `XamLoaderTerminateTitle` only when that gate reports its terminal
   condition;
4. invokes `0x821f7bc8` and `0x821f7ae8`, which iterate bounded callback
   ranges in the same lifecycle area.

This is a platform/CRT-style lifecycle slice. It does not identify a game
title state, a resource archive, a renderer, or a stable mod hook. The raw
entry code is needed because Ghidra's helper subentry splitting truncates some
decompiled call graphs.

## Reproduction

```sh
JAVA_TOOL_OPTIONS='-Duser.home=/tmp/ac6-ghidra-home' \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript VerifyEntryLifecycleCalls.java -noanalysis
```

Next action: find the first direct call after this lifecycle region whose
static dependencies include a title resource, archive, or game-owned object.
