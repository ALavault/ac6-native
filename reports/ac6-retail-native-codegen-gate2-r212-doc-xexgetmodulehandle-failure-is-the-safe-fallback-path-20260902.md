# AC6 retail NTSC-U/J — documentation only: `XexGetModuleHandle`/`XexGetProcedureAddress` failure is the safe fallback path (r212)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. **No code change this cycle** — documentation only, same class as
r164/r178/r202/r211. `ctest`/pytest unaffected: 196/196 pytest, 10/10
ctest (last verified at r210, unchanged since no source was touched).

## What was found: current failure is the correct, safe behavior

Traced both real call sites of `XexGetModuleHandle`/
`XexGetProcedureAddress` (`Function_821FCCE0` and its sibling at
`0x821fced0`). Both follow the identical shape:

```
XexGetModuleHandle("xam.xex", &moduleHandle)   ; only attempted above a
                                                  version-gate check
if success:
  XexGetProcedureAddress(moduleHandle, ordinal, &procAddress)
if procAddress resolved:
  bctrl (call the dynamically-resolved function pointer)
else:
  bl <fixed, statically-linked fallback address>
```

This is the standard Xbox 360 forward-compatibility pattern: a title
probes for a newer XAM export (gated by dashboard version) and, if the
newer export isn't available, falls back to a fixed, already-linked
static implementation of the same functionality. `kOfflineStatus`
(negative) makes both calls fail unconditionally, which routes every one
of these call sites through the **static fallback path** — the same
code path a real, older dashboard would also take. This is not a
neglected bug; it is the intended, safe branch of a pattern this project
does not need to implement the dynamic side of at all, since correctly
"succeeding" here would require synthesizing a real, guest-callable
function pointer for the resolved export — a substantially riskier
undertaking (a wrong or synthetic pointer fed to `bctrl` risks a genuine
crash) for no behavioral gain over the fallback that already runs today.

Confirmed adequate; no fix needed, and none is planned unless a future
call site is found that lacks an equivalent static fallback.

## Also checked, no new fix: `XamContentCreateEx`

Single real call site (`0x821f547c`, inside `Function_821F5430`) is
gated by several parameter-shape validations that return
`ERROR_INVALID_PARAMETER` (`0x57`) before ever reaching the import if
they fail. When reached, this is a content-package open/create call —
squarely inside the same "save/reload" territory r202 already traced and
deferred (real write support to `NativeGuestMediaService` is the actual
prerequisite, not a contract-shape fix to this one import). Not
attempted further this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r212). Most of what remains on the generic-fallback
   list now falls into already-documented larger buckets: `NetDll_*`
   (networking, ~33 imports, out of scope without a real network stack),
   SEH (`RtlRaiseException`/`RtlUnwind`/`RtlCaptureContext`), the
   `XMsg*` message-dispatch family (r203), the save-write path
   (`NtWriteFile`/`NtDeviceIoControlFile`/`NtSetInformationFile`/
   `NtQueryInformationFile`, r202), `sprintf`/`_vsnprintf` (varargs
   engine), and the `XamGetExecutionId`-gated identity/profile cluster
   (r211).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
