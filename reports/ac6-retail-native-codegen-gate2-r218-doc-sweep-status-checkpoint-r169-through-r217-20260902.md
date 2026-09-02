# AC6 retail NTSC-U/J — documentation only: offline-import sweep checkpoint, r169-r217 (r218)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. **No code change this cycle** — documentation only, same class as
r164/r178/r202/r211/r212. `ctest`/pytest unaffected: 203/203 pytest, 10/10
ctest (last verified at r217, unchanged since no source was touched).

## Why a checkpoint now

The offline-import sweep begun at r148 and continued through r217 has
gone from 125 generic-fallback imports (measured at the start of this
session's continuation) down to 87. Every remaining candidate checked
this cycle (`XeKeysConsoleSignatureVerification`, `XamTaskCloseHandle`
re-examined) confirmed the same pattern already established: either a
genuine cryptographic dependency this project cannot fabricate, or a
fix that would be inert without a larger prerequisite this session has
already declined to build. Individual small, cleanly-bounded wins
outside the buckets below are now sparse enough that continuing to
search for them at the same pace has diminishing returns; this
checkpoint exists so the next cycle (in this session or a future one)
starts from an accurate map rather than re-discovering it.

## What's been fixed (r169-r217), by category

- **Real bugs with observable consequences**: `KfAcquireSpinLock`
  family, semaphore primitives, `KeBugCheck`/`XamLoaderTerminateTitle`/
  `ExTerminateThread`/`HalReturnToFirmware` (never-returns), `XamAlloc`,
  `XNotifyGetNext` (uninitialized-memory branch), `NtOpenFile` (9 call
  sites), `ObCreateSymbolicLink`/`Delete` (boot-sequence blocker),
  `XAudio*RenderDriverClient` family (init-order blocker),
  `XamVoiceHeadsetPresent`, `XamNotifyCreateListener`,
  `NtSetTimerEx`/`NtCancelTimer`/`NtCreateTimer` (full real timer
  subsystem), `VdSetDisplayMode`.
- **Ignored-return cosmetic fixes** (no observable behavior change,
  cleared trace noise): `KeLockL2`/`KeUnlockL2`/`KiApcNormalRoutineNop`,
  `IoDismountVolume` family, `XamVoiceClose`, `XMsgCancelIORequest`,
  `ExRegisterTitleTerminateNotification`, `XMACreateContext`/
  `ReleaseContext`.
- **Corrections to earlier cycles' own mistakes** (per this project's
  evidence discipline): r203 corrected r197's mischaracterization of
  `0x823cfe4c` as internal telemetry (it is the real
  `XMsgStartIORequest` import); r209/r211 escalated `XamGetExecutionId`'s
  known blast radius and corrected its signature to a pointer-to-pointer.
- **Confirmed already-adequate, no fix needed**: `XamUserAreUsersFriends`
  (never reads its own return value), `XexGetModuleHandle`/
  `XexGetProcedureAddress` (failure is the intended forward-compat
  fallback path), `XamVoiceCreate`/`XamVoiceSubmitPacket` (current
  failure is honest absent real microphone hardware).

## What remains, by bucket (the ~87 still on the generic-fallback list)

1. **Networking** (`NetDll_*`, ~29 imports): needs a real socket/network
   stack. Not attempted; out of scope without one.
2. **Structured exception handling** (`RtlRaiseException`/`RtlUnwind`/
   `RtlCaptureContext`, `__C_specific_handler` — the last has 0 real call
   sites XEX-wide, confirmed unreachable): needs a full SEH dispatch/
   unwind engine this project does not have.
3. **Save-write path** (`NtWriteFile`/`NtDeviceIoControlFile`/
   `NtSetInformationFile`/`NtQueryInformationFile`): real shape traced at
   r202 (a genuine FATX chunked-write save file writer), needs real write
   support in `NativeGuestMediaService` (currently read-only).
4. **`XMsg*` message-dispatch family** (`XMsgStartIORequest`,
   `XMsgInProcessCall`, `XMsgStartIORequestEx`, and the
   `XamSessionCreateHandle`/`RefObjByHandle` traffic that rides on it):
   r203 confirmed this is real IPC-style dispatch with 17+ call sites and
   multiple message types, not telemetry — a multi-cycle undertaking to
   enumerate correctly.
5. **Varargs formatting** (`sprintf`/`_vsnprintf`): needs a small
   guest-ABI printf engine, not a contract fix.
6. **`XamGetExecutionId`-gated identity/profile cluster**
   (`XamGetExecutionId` itself, `XamUserReadProfileSettings`,
   `XamUserCreateStatsEnumerator`, `XamUserCreateAchievementEnumerator`,
   and the differently-gated `XamUserGetXUID`/`XamUserGetSigninInfo`):
   r211 confirmed the real signature and blast radius; needs either a
   confirmed `XAM_EXECUTION_INFO` field layout or understanding of a
   return-value bit-masking pattern neither traced to ground truth yet.
7. **Untraced UI-dialog trampolines** (`XamShowSigninUI` and 6 siblings,
   `XamEnumerate`, `XamWriteGamerTile`, `XamLoaderLaunchTitle`,
   `XamContentCreateEnumerator`/`CreateEx`/`Delete`/`GetDeviceData`/
   `GetDeviceState`/`SetThumbnail`, most with 0-1 real call sites): each
   routes through a one-instruction tail-jump trampoline; determining a
   safe default requires tracing each trampoline's own caller
   individually, not done for all of them yet.
8. **Permanently out of reach**: `XeKeysConsolePrivateKeySign`/
   `XeKeysConsoleSignatureVerification` (a real console hardware secret;
   re-checked this cycle, same conclusion as r202 — not a "needs more
   time" item, a genuine hardware-secret dependency).
9. **Genuinely unreachable, no action needed**: `StfsControlDevice`/
   `StfsCreateDevice`/`NtQueryDirectoryFile`/`NtReadFileScatter`/
   `__C_specific_handler` (0 real call sites XEX-wide, confirmed via
   exact-symbol search) — leaving these as the generic no-op is correct,
   not neglect.
10. **Inert without its own prerequisite**: `XamTaskCloseHandle` (r198's
    `XamTaskSchedule` deferral already makes this call site unreachable;
    re-confirmed this cycle, still true).
11. **`NtDuplicateObject`**: re-examined this cycle with the lens of
    "wrapper functions adapt reduced args to real signatures" that
    resolved r216's timer signature. Its one real call site
    (`0x82390be0`) only supplies 3 of the real 7-argument signature's
    registers (`r3`-`r5`); `r6` (the real `TargetHandle*` output pointer
    position) is never deliberately set by this caller, so if the real
    API writes through it, a naive fix risks a wild write through
    whatever garbage value happens to sit in that register — a real,
    not hypothetical, correctness risk. Still deferred; the caller's own
    earlier register history would need tracing before this is safe.

## Gates

No code change; pytest 203/203 and `ctest` 10/10 as of r217, unaffected.

## Next

1. If this sweep continues, the highest-value remaining single item is
   probably #6 (the `XamGetExecutionId` cluster, 7+ confirmed call
   sites) — it needs one more piece of evidence (an independently
   confirmed struct field or a second real call site using the same
   wrapper with a traceable comparison value) rather than a fresh
   subsystem.
2. The other buckets (#1-#5) are all legitimate subsystem-building
   projects in their own right, not sweep continuations — worth
   revisiting only if/when the user wants to invest in one specifically.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
