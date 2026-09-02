# AC6 retail NTSC-U/J — documentation only: `XamSessionCreateHandle`/`XamSessionRefObjByHandle` fully traced, already adequate (r232)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle.** `ctest` 10/10, pytest 209/209
(unaffected — no source touched).

## What was found

r197 flagged `XamSessionCreateHandle`/`XamSessionRefObjByHandle` as
"pas assez tracé" (not traced enough) to decide a fix. This cycle traced
both fully.

`XamSessionCreateHandle` has exactly one real caller
(`Function_821FD4C0`, `0x821fd598`): `uVar1 =
func_0x823d08ec(param_8); if (uVar1 != 0) { return uVar1; }` — the
caller checks the status and returns the failure honestly if non-zero,
before ever touching the handle it would have written.

`XamSessionRefObjByHandle` has eleven real callers
(`0x821fd890`, `0x821fd7e8`, `0x821fd698`, `0x821fdb58`, `0x821fdbf4`,
`0x821fd5b4`, `0x821fd734`, `0x821fddb8`, `0x821fdaa0`, `0x821fd934`,
`0x821fd9dc`). All eleven were decompiled. Every one follows the
identical shape: `uVar = func_0x823d08dc(handle, &obj_out); if (uVar ==
0) { ...use obj_out, eventually call XMsgStartIORequest (r203)... }` (or
the equivalent `if (uVar != 0) return uVar;` early-exit form). Not one
caller uses `obj_out` without first confirming the status is zero, and
every caller returns the non-zero status honestly instead of proceeding.

Most of these callers go on to call `func_0x823cfe4c` — the real
`XMsgStartIORequest` import r203 already identified — with a `MessageType`
constant like `0xb0010`/`0xb0012`/`0xb0018`, confirming this is the real
`XamSession*`/`XMsg` IPC-request-construction family r203 flagged as
broader in scope than r197's original characterization, but that breadth
does not translate into a contract-shape bug for either import in this
pair: both already behave correctly under the generic offline default,
because every real caller treats a non-zero status as an honest,
gracefully-handled failure.

## Consequence

This closes the pair r197 left open. No fix is needed for either import:
the generic `kOfflineStatus` response is already the correct, safe
behavior at every one of the twelve real call sites traced (1 for
`XamSessionCreateHandle`, 11 for `XamSessionRefObjByHandle`).

## Next

1. Do not re-open this pair without new evidence — it is now fully traced
   at every real call site, not sampled.
2. Remaining unexamined-in-this-sweep imports from r218's catalog:
   `NtDuplicateObject` (r197, deliberately deferred — real risk of a wild
   write, re-confirmed since), `XamGetExecutionId` (r219, confirmed
   always bypassed — no fix needed, same "already adequate" class as this
   cycle's finding), `XamUserAreUsersFriends` (r211, confirmed adequate),
   `XamUserCreateAchievementEnumerator`/`XamUserCreateStatsEnumerator`
   (r219, confirmed adequate), `XamVoiceCreate`/`XamVoiceSubmitPacket`
   (r207/r210, deferred — dependent handle chain), `XexCheckExecutablePrivilege`
   (r178, deferred — privilege semantics not locally determinable),
   `XexGetModuleHandle`/`XexGetProcedureAddress` (r212, confirmed
   adequate). The `XamShow*` UI-dialog cluster remains closed per
   r224/r225 (no stub-level fix applies). `_vsnprintf`/`sprintf` (varargs
   printf engine) and the SEH triad (`RtlCaptureContext`/
   `RtlRaiseException`/`RtlUnwind`) remain out of a single bounded
   cycle's scope, per r202.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
