# AC6 cycle 9 — XEX preflight context

## Qualified evidence

- target: `ace-combat-6-pal`
- XEX SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- functions: `0x821f7d10`, `0x821f7de8`, `0x821f9820`
- method: bounded Ghidra `-noanalysis` reads and mnemonic assertion

## Static slice

`0x821f7d10` obtains `RtlImageXexHeaderField(module, 0x20401)` when an
executable module handle exists. If the returned field is absent or nonzero,
it initializes `DAT_82a5ee5c` only once through `0x821f9820` and returns whether
that persistent pointer is non-null. Otherwise it returns true directly.

`0x821f9820` is a platform/runtime-context initializer, not a game-system
function: its static dependencies include `NtAllocateVirtualMemory`,
`NtQueryVirtualMemory`, `NtFreeVirtualMemory`, `KeGetCurrentProcessType`, and
`RtlInitializeCriticalSection`. The caller `0x821f7de8` invokes
`HalReturnToFirmware(1)` only when this boolean preflight fails.

This establishes an entry path boundary of `ABI save helper -> XEX header
preflight -> platform context`. It does not establish a title, asset, input,
renderer, or game-loop path.

## Reproduction

```sh
JAVA_TOOL_OPTIONS='-Duser.home=/tmp/ac6-ghidra-home' \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript VerifyXexPreflightPath.java -noanalysis
```

Next action: identify the first post-preflight call that reaches title/resource
code, and only then schedule a Xenia breakpoint/trace.
