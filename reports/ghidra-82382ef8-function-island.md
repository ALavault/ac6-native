# AC6 `0x82382ef8` bounded Ghidra helper-subentry inspection

## Scope

This is a read-only static inspection of the active, qualified PAL XEX Ghidra
project. It does not change a function boundary, a `noreturn` property, a
symbol, or generated Xenon output.

- target: `ace-combat-6-pal`
- binary: `game-files/default.xex`
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- loader image base: `0x82000000`
- inspected range: `0x82382ec0` through `0x82382f80`

## Reproduction

```sh
JAVA_TOOL_OPTIONS='-Duser.home=/tmp/ac6-ghidra-home' \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript InspectFunctionIsland.java 82382ec0 82382f80 \
  -noanalysis
```

## Corrected result

The current Ghidra function manager splits the following ABI helper into
four-byte function bodies. Raw instruction inspection shows that they are
sequential instructions, not independent noreturn functions. In particular,
`FUN_82382ef8` is the `r28` subentry:

```text
0x82382ef8: std r28,-0x28(r1)
```

The sequence from `0x82382ec0` stores `r14..r31` and LR and returns at
`0x82382f0c`; `0x82382f10..0x82382f60` restores those values. The active
project's `noreturn` label on the short fragments is therefore a boundary/type
artefact. It explains why the entry-wrapper export stops at its first call even
though the raw instructions continue.

## Consequence

Keep `0x821f5e90` as the statically confirmed entry wrapper. Reclassify
`0x82382ef8` as a PPC ABI callee-save helper subentry; do not use its raw
exported callers as architectural fan-in and do not reconstruct it as game
code.

Next evidence: trace the first call after the helper, `0x821f7de8`, through
`0x821f7d10` in Xenia/XenonTests before selecting a game-owned boot function.
