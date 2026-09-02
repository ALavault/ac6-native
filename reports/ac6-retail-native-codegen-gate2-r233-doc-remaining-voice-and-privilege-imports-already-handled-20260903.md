# AC6 retail NTSC-U/J — documentation only: `NtDuplicateObject`/`XamVoiceCreate`/`XamVoiceSubmitPacket`/`XexCheckExecutablePrivilege` re-confirmed, no fix warranted (r233)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle.** `ctest` 10/10, pytest 209/209
(unaffected — no source touched).

## What was found

Continuing the sweep with the smaller set of remaining candidates named by
r232, this cycle re-traced four previously-deferred imports with the same
tooling used throughout r225–r232, to confirm whether newer evidence
changes any of their prior dispositions. It does not, for any of the four.

**`NtDuplicateObject`** (r197's original finding): one real call site,
`Function_82390BC8` (`0x82390be0`). Decompiled: `iVar1 =
func_0x823d036c(...); if (iVar1 < 0) { Function_821F75B8(); } return
iVar1 >= 0;`. The caller already handles a negative status gracefully —
the generic offline default is safe here. r197's separate concern (a
naive real *implementation* risks a wild write, since the traced
`TargetHandle`-equivalent register position at this call site carries a
reused handle value, not a valid memory address) is about implementing a
real duplication, not about the current default's safety — that concern
stands unchanged and still blocks a real fix.

**`XamVoiceCreate`** (r207): one real call site,
`Function_822075F8` (`0x82207620`). Confirmed: `lVar4 =
func_0x823d093c(...); if (-1 < lVar4) { ...use handle... }` — the caller
gates all handle use on a non-negative status, so the generic negative
default is already safe. Matches r207 exactly; no new finding.

**`XamVoiceSubmitPacket`** (r210): two real call sites (`0x8220a5e0`,
`0x82207368`), both decompiled this cycle for the first time. Both check
`iVar < 0` before treating the call as failed, and both operate on a
handle field only ever populated by `XamVoiceCreate` — which, per the
above, never produces a real handle. The generic default is safe at both
sites; r210's "fixing this alone would be inert without the whole voice
subsystem" stands confirmed, now with both call sites traced rather than
inferred.

**`XexCheckExecutablePrivilege`** (r178): considered and explicitly
declined again this cycle. All 3 real call sites (`0x821f5d08`,
`0x82391ac8`, `0x82391d74`, the latter two both inside
`Function_82391A40`) test the result as a plain boolean
(`result != 0`), and the generic offline default (`kOfflineStatus`,
non-zero) currently reads as "privilege granted" at every site — this
was already r178's own exact finding. It was tempting to treat "the
callers use it as a boolean, so an offline/unsigned title should
honestly report no privilege (0)" as a derivable fix, but r178 already
weighed this precisely and declined: there is no call-site evidence in
this XEX pinning which answer (granted/denied) is real-hardware-correct
for these specific privilege IDs (`0xa`, `0x17`), and switching to
"denied" risks introducing a bail path that does not currently execute —
the riskier, less-tested change, per this project's own precedent
(cycles 1111/1113: "a plausible rule with no control is refused"). No new
evidence surfaced this cycle to overturn that reasoning, so it stands
unchanged.

## Consequence

None of these four imports get a fix this cycle. Three are confirmed
already-adequate (no behavior change possible or needed); one
(`XexCheckExecutablePrivilege`) remains correctly deferred for lack of a
control case, not for lack of tracing effort.

## Next

1. Do not re-open `NtDuplicateObject`/`XamVoiceCreate`/
   `XamVoiceSubmitPacket` without new evidence — all real call sites are
   now traced, not sampled.
2. `XexCheckExecutablePrivilege` needs an external, out-of-band decision
   (or a genuinely new piece of in-XEX evidence pinning the correct
   answer) before it can move, not another re-trace.
3. With this cycle, the offline-import sweep has now individually
   examined essentially every remaining candidate in r218's original
   catalog. What's left unfixed falls into four categories, all already
   named: (a) imports needing a subsystem this project has deliberately
   not built (`XamTaskSchedule`, save/reload write support, the voice
   handle chain); (b) renderer-policy-blocked imports
   (`VdGetSystemCommandBuffer`/`VdPersistDisplay`); (c) permanently
   out-of-scope imports (`XeKeysConsolePrivateKeySign`/
   `XeKeysConsoleSignatureVerification`); (d) imports where no control
   case exists to pin a correct answer (`XexCheckExecutablePrivilege`,
   `NtDuplicateObject`'s real-implementation path). `_vsnprintf`/
   `sprintf` (a varargs printf engine) remain the one candidate that is
   neither dead, adequate, nor blocked — just larger than a single
   bounded cycle, per r192.
4. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
