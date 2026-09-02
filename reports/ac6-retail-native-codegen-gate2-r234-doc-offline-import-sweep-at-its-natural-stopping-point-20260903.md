# AC6 retail NTSC-U/J — documentation only: `sprintf`/`_vsnprintf` scoped, the offline-import sweep is at its natural stopping point (r234)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle.** `ctest` 10/10, pytest 209/209
(unaffected — no source touched).

## `sprintf`/`_vsnprintf` — scoped, confirmed too large for a bounded cycle

`sprintf` has 7 real call sites (`0x821ef6fc`, `0x821ef78c`,
`0x821d9cf8`, `0x821ea06c`, `0x821ea158`, `0x821ea218`, `0x821eb088`);
`_vsnprintf` has 2 (`0x821ef49c`, `0x821ef528`). Four of the seven
`sprintf` sites were decompiled this cycle. They are save-slot/file-path
diagnostic formatting (inside functions already recognizable from r202's
save/reload tracing — `Function_821E9F50` builds save-directory paths
and enumerates slot files) using a heterogeneous mix of format
specifiers across different calls (`%s`-style path/string substitution,
integer formatting for slot indices, and at least one call whose format
string is read indirectly rather than a fixed literal at the call site
itself). This is not a small, fixed set of format strings this sweep's
bounded per-import methodology can special-case — implementing it
correctly requires a real varargs-parsing printf engine (specifier
parsing, width/precision, at minimum `%s`/`%d`/`%x` with PPC varargs
register-and-stack argument fetching), matching r192's own prior
characterization of this exact pair as "hors scope d'un cycle borné."
This cycle's tracing narrows *where* the calls are (save/reload
diagnostic paths, not gameplay-critical) without changing that
conclusion.

## The offline-import sweep (r148–r233) is at its natural stopping point

Of the ~125 generic offline-import stubs present at the start of this
sweep, dozens have been given real, evidence-derived fixes (r169–r230),
and every other remaining candidate has now been individually traced at
least once, with a settled disposition:

- **Confirmed dead** (zero real callers in this XEX): the `XamContent*`
  cluster, `XamWriteGamerTile`, `NtQueryDirectoryFile`,
  `NtReadFileScatter`, `Stfs{Control,Create}Device`,
  `XamLoaderLaunchTitle`, `XamContentCreateEx`, `XamEnumerate`,
  `__C_specific_handler`, and 26 of the 29 `NetDll_*` networking imports
  (r229, r230, r231).
- **Confirmed already adequate** (real callers already handle the
  generic negative status gracefully, so no behavior change is possible
  or needed): `NtSetInformationFile`, the 3 remaining live `NetDll_*`
  imports, `XamSessionCreateHandle`/`XamSessionRefObjByHandle`,
  `NtDuplicateObject`, `XamVoiceCreate`, `XamVoiceSubmitPacket`,
  `XexGetModuleHandle`/`XexGetProcedureAddress` (r212),
  `XamUserAreUsersFriends` (r211), `XamGetExecutionId` (r219),
  `XamUserCreateAchievementEnumerator`/`XamUserCreateStatsEnumerator`
  (r219) (r229, r231, r232, r233).
- **Blocked on a subsystem this project has deliberately not built**:
  `XamTaskSchedule` (guest-callback execution), the save/reload write
  path (`NtWriteFile`/`NtDeviceIoControlFile`, r202), the voice-handle
  chain (already covered above as adequate-but-inert without a real
  `XamVoiceCreate`), `sprintf`/`_vsnprintf` (this cycle).
- **Blocked on renderer policy**: `VdGetSystemCommandBuffer`/
  `VdPersistDisplay` — the native Vulkan backend is deliberately
  fail-closed pending oracle-free microcode qualification.
- **Permanently out of scope**: `XeKeysConsolePrivateKeySign`/
  `XeKeysConsoleSignatureVerification` (r202).
- **No control case to derive a fix from**: `XexCheckExecutablePrivilege`
  — re-examined and explicitly declined again this cycle (r233), same as
  r178's original finding.
- **Closed as out of scope for this stub sweep entirely** (real, reachable
  internal `.text` addresses, not offline-import stub gaps): the two
  `XamShow*`-adjacent trampolines r225 traced, and the dispatch-table
  cluster r224/r225 investigated.

There is no remaining candidate in r218's original catalog that fits this
sweep's methodology (a bounded, single-import, evidence-derived contract
fix) and has not already been given one, or correctly declined one with
a named reason.

## Next

1. This specific "sweep the offline-import stub list" line of work has no
   further bounded candidates. Reopening any of the above requires new
   evidence (a different game-mode entry point reaching a currently-dead
   import, a control case for `XexCheckExecutablePrivilege`, or an
   explicit go/no-go decision to build one of the named subsystems), not
   more re-tracing of the same call sites.
2. Gate 2's own two other named frontiers remain exactly where they were
   before this sweep began, and neither has moved as a result of it:
   DATA.TBL chain fully traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V
   translation policy-blocked (no oracle for the whole campaign).
3. `NEXT.md`'s standing "prochaine décision" items 1 and 2 (runtime
   confirmation of the r180 input-backend fix with real hardware; tracing
   r190's second validation constant if it is ever observed to fail) both
   require an external resource (a physical controller, or a runtime
   observation) this session does not have — they are not actionable
   without one.
4. A future cycle with a genuine go/no-go decision to build the
   save/reload write path, the guest-callback execution mechanism for
   `XamTaskSchedule`, or a real varargs printf engine would be the next
   substantive frontier for Gate 2's native-import surface; none of these
   is a bounded single-cycle fix, and starting one without an explicit
   scoping decision would violate this project's own discipline against
   inventing scope.
