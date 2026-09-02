# AC6 retail NTSC-U/J — documentation only: a large unreached-import cluster, and `NtSetInformationFile` already adequate (r229)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle.** `ctest` 10/10, pytest 208/208
(unaffected — no source touched, not re-run this cycle since nothing
changed).

## What was found

Continuing r228's candidate list, this cycle checked every remaining
`Xam*`/`Nt*`/`Stfs*` import from the r218 catalog not yet individually
traced, using `scripts/FindSymbolReferences.java` first and
`scripts/ReferencesTo.java` on any trampoline found.

**Zero real callers, confirmed dead in this build:**

- `XamWriteGamerTile` — trampoline `0x821f458c` exists, zero callers.
- `XamContentGetDeviceState`, `XamContentGetDeviceData`,
  `XamContentClose`, `XamContentDelete`, `XamContentSetThumbnail`,
  `XamContentCreateEnumerator` — none of these six have even a labeled
  trampoline reference; the import label itself is never referenced
  anywhere in this XEX.
- `NtQueryDirectoryFile`, `NtReadFileScatter`, `StfsControlDevice`,
  `StfsCreateDevice` — same: the import label itself has no reference at
  all.
- `XamLoaderLaunchTitle` — trampoline `0x821f5ba0` exists, but its only
  two references (`0x821f5b1c`, `0x821f5b94`) are `CONDITIONAL_JUMP`
  instructions **inside the trampoline's own function body** (the same
  internal-jump-table shape r225 found for one of the closed dispatch
  -table entries) — no external caller.
- `XamContentCreateEx`, `XamEnumerate` — trampolines exist
  (`0x821f547c`, `0x821f48b4`), zero references of any kind.

This is a large, genuinely unreached cluster in this qualified build —
consistent with the "save/reload gated closed" and "content/marketplace
UI not exercised by Mission 01" characterizations r202/r206/r212 already
established for adjacent imports. No fix is possible or meaningful for an
import with zero real callers; there is nothing to derive a contract from.

**`NtSetInformationFile` (9 real call sites, the highest count checked
this sweep): confirmed already adequate, no fix needed.** Four of the
nine were decompiled (`0x821f5714`/`Function_821F56A0`,
`0x821f7460`/`Function_821F73F8`, `0x823908cc`+`0x823908f4`
/`Function_82390880`, `0x823921e4`/`Function_82392040`). Every one
follows the same shape already established for the save/reload path
(r202): it gates its own success strictly on the returned status being
non-negative (`if (-1 < iVar1) { ... } else { Function_821F75B8(...);
return failure; }`), and takes an honest, graceful failure path
otherwise. `Function_82392040` is itself part of the write-heavy
save-file-construction chain r202 already named (calls the same
`func_0x823d022c`/`0x823d023c` write-path functions as
`Function_82392878`). The generic offline default's `kOfflineStatus`
(negative) is therefore already the correct, safe response at every
traced call site: it produces an honest save/write failure, not a wild
write or ignored-return risk. This does not contradict r202 — it
confirms `NtSetInformationFile` belongs to the same still-deferred
save/reload frontier, not a separate contract-shape bug.

## Consequence

The offline-generic-import count (82 after r228) is reduced in *scope*,
not in raw count: none of the eleven dead imports found this cycle can be
fixed (nothing to fix), and `NtSetInformationFile` needs no fix (already
adequate). This narrows what remains genuinely open in r218's catalog.

## Next

1. Do not re-attempt a fix for any of the eleven confirmed-dead imports
   listed above without new evidence that they become reachable (e.g. a
   different mission or game-mode entry point) — repeating this trace
   would be pure waste.
2. `NtSetInformationFile`'s adequacy reinforces (does not reopen) r202's
   save/reload frontier: `NtOpenFile`→`NtDeviceIoControlFile`→loop
   `NtWriteFile`/`NtSetInformationFile` in
   `Function_82392878`/`Function_82392040` remains the real binary shape
   to start from if that frontier is ever taken up.
3. Continue the offline-import sweep per r218's catalog for the remaining
   untraced imports (`__C_specific_handler`, `_vsnprintf`, `sprintf`, the
   networking cluster, SEH, `XeKeys*`, `XexGetModuleHandle`/
   `XexGetProcedureAddress` (already confirmed adequate by r212),
   `XamTaskSchedule`/`XamTaskCloseHandle`, `VdGetSystemCommandBuffer`/
   `VdPersistDisplay` (renderer-policy territory)).
4. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
