# AC6 retail NTSC-U/J — documentation only: corrects r209/r211 — the `XamGetExecutionId` gate is always bypassed (r219)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. **No code change this cycle** — documentation only, same class as
r164/r178/r202/r211/r212/r218. `ctest`/pytest unaffected: 203/203 pytest,
10/10 ctest (last verified at r217, unchanged since no source was
touched).

## Correction: the wrapper's gate is bypassed at every real call site

r209 and r211 both described the wrapper at `0x821f7668` (built around
`XamGetExecutionId`) as gating `XamUserReadProfileSettings`,
`XamUserCreateStatsEnumerator`, and `XamUserCreateAchievementEnumerator`
— treating the wrapper's own check argument as a live, sometimes-nonzero
value that could route through the (currently always-failing)
`XamGetExecutionId` call. This cycle traced that check argument back one
more level, to the actual functions that call the intermediate wrappers
(`Function_821FE2C8`/`Function_821FE330` for `XamUserReadProfileSettings`,
`Function_821F44B8`/`Function_821F4518` for
`XamUserCreateStatsEnumerator`, `Function_821F4590` for
`XamUserCreateAchievementEnumerator`), confirming via
`scripts/FindSymbolReferences.java` that the wrapper at `0x821f7668` has
exactly 5 real callers total, and reading each one's own caller in turn.

**Every one of the 5 real call sites passes a literal `li r3,0x0`** as
the check argument (`0x821ce7a0`/`0x821ce7cc` for the two
`XamUserReadProfileSettings` wrapper variants, `0x821710a4`/`0x8217119c`
for the two `XamUserCreateStatsEnumerator` wrapper variants, `0x821cee9c`
for `XamUserCreateAchievementEnumerator`). The wrapper's own logic
(`cmplwi cr6,r31,0x0; beq cr6,<success-shortcut>`) means a zero check
value **always** takes the shortcut straight to success, without ever
calling the real `XamGetExecutionId` import at all. So `XamGetExecutionId`
being broken has **zero observed effect anywhere in this XEX** — not a
partial gate with unconfirmed edge cases, a gate this project's own real
code never actually exercises.

## Consequence: all three "gated" imports are confirmed already-adequate, not deferred

With the gate confirmed bypassed, each import's own reachability and
current behavior stands on its own:

- `XamUserCreateStatsEnumerator`/`XamUserCreateAchievementEnumerator`:
  both intermediate wrappers pass the real import's raw return straight
  up to their own callers unchanged, and every one of those callers
  checks `cmplwi cr6,r3,0x0; bne <skip-cleanly>` — **any** nonzero
  return (including the generic `kOfflineStatus`) is already treated as
  a clean, graceful "feature unavailable" skip, not a crash or corrupted
  state. This project has no real stats/achievement data to report
  regardless, so the current no-op failure is the honest outcome
  already — same "confirmed already adequate" bucket as
  `XamUserAreUsersFriends` (r211) and `XexGetModuleHandle`/
  `XexGetProcedureAddress` (r212).
- `XamUserReadProfileSettings`: its own callers check for **specific**
  nonzero codes (`0x7a`/`122` and `0x3e5`/`997`, the latter
  `ERROR_IO_PENDING`) rather than any-nonzero, following the same
  async-completion contract already identified for
  `XamShowMessageBoxUIEx` (r206). One traced call site passes
  `dwNumSettingIds=0`/`pdwSettingIds=NULL` — a degenerate "probe" call,
  not a real settings read. This import's real blocker is that async
  contract, not the (bypassed) `XamGetExecutionId` gate; still deferred,
  but for the corrected reason, and not attempted further this cycle
  since this project has no overlapped-wait subsystem to complete it
  through, the same limitation r206 already named for
  `XamShowMessageBoxUIEx`.

## Next

1. `XamGetExecutionId` itself can be deprioritized in this project's own
   tracking: it is still technically a generic no-op, but this cycle
   found no real call site anywhere in this XEX where that matters.
2. `XamUserReadProfileSettings` remains a real, scoped candidate if this
   project ever builds an overlapped-wait completion mechanism (which
   would also unblock `XamShowMessageBoxUIEx`) — not a struct-layout
   problem as r206/r211 suggested.
3. Continue the broader offline-import sweep per r218's bucket catalog.
4. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
