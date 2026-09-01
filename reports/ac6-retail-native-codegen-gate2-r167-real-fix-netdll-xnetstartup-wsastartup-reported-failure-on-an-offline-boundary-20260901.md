# AC6 retail NTSC-U/J — real fix: `NetDll_XNetStartup`/`NetDll_WSAStartup` reported failure where the offline boundary should succeed (r167)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, `-readOnly -noanalysis`. XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with matching tests. Verified via a clean `tools/build.py --target
ntsc-uj --profile native` (`ctest` 9/9), the full retail-native pytest
suite (150/150, up from 148/148), and a live import trace confirming both
imports no longer reach the generic offline-import fallback.

## What was found

Continuing the offline-import audit (r162-r165's family), this cycle
checked `NetDll_XNetStartup`/`NetDll_WSAStartup` — network-stack
initialization calls this project's own established convention already
treats as "offline-only HLE boundary; no socket or host I/O side effect"
for other functions in this file. Neither had a dedicated case; both fell
through to the generic fallback.

`Ac6Xrefs.java` against each real import thunk address (`0x823D06BC`/
`0x823D077C`) finds one wrapper function per import
(`sub_821FCCE0`/`sub_821FCED0`), each reached only via an internal branch
from a sibling entry point rather than a literal `bl` from elsewhere in
the XEX — the actual external call site is indirect (the same
computed-call shape r156 found for a different import), not re-derived in
full this cycle. What *is* directly established from full disassembly of
both wrappers: each one calls the import and returns its value completely
unmodified as its own result —

```
821fcd7c  bl 0x823d06bc      ; NetDll_XNetStartup
821fcd80  addi r1,r1,0x70    ; epilogue begins immediately; r3 untouched
...
821fcd94  blr                ; returns exactly what the import returned
```

The real WinSock/XNet convention for both calls is `INT` — `0` on
success, nonzero on failure. The generic offline-import fallback's
`kOfflineStatus` (`0xC00000BB`) is nonzero, which a caller checking
`== 0` (the standard convention, and the one this project's own comments
already describe for parallel offline-only stubs) reads as failure — the
wrong outcome for a stub with no real network condition to actually fail
on.

## Fix

Both now return `0u` (success) instead of `kOfflineStatus`, matching this
project's own already-established pattern of succeeding past an absent
network rather than reporting a failure with no real cause. This is not
full network functionality — no socket or host I/O is added — only the
return-value shape at this offline boundary.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 150/150
(148/148 before this cycle, +2 new tests: a parametrized
`test_net_startup_reports_success_not_a_status` covering both imports).
Live import trace confirms neither reaches `trace_offline_import()` any
more. `gdb` backtrace confirms the tracked `sub_821F7C80` crash still
reproduces identically — this fix runs well before that chain and is not
expected to (and does not) change it. `git status` clean apart from the
intended change set.

## Next

Both of Gate 2's named frontiers are unchanged: DATA.TBL chain fully
traced and closed at both the value and mechanism level (r144/r153/r161/
r166); `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked. The indirect call site
reaching `sub_821FCCE0`/`sub_821FCED0` was not traced this cycle (deferred,
same open question as r156's `sub_821F5630` case) — not required for this
fix, since the wrapper's own transparent pass-through already establishes
the return value matters without needing to know exactly who reads it.
