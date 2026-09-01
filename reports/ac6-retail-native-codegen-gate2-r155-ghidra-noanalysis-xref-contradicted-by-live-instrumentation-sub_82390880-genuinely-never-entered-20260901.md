# AC6 retail NTSC-U/J — a `-noanalysis` Ghidra static xref result is directly contradicted by live instrumentation; `sub_82390880` is genuinely never entered this run (r155)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. One `-readOnly -noanalysis` Ghidra headless run using the
existing `tools/ghidra_scripts/Ac6Xrefs.java` (real cross-reference-manager
lookup, not text matching). One unconditional, throwaway diagnostic
(`fprintf`, no env gate this time, to eliminate any possibility of an
environment-variable issue) added to a gitignored, regenerated build-tree
file (`generated/ppc_recomp.66.cpp`), fully reverted via `cp` from a
`/tmp/*.orig` backup, confirmed via `grep -c "r155"` returning 0, and a
clean `tools/build.py --target ntsc-uj --profile native` (`ctest` 9/9).
`git status` unchanged from r154's own qualification.

## What was attempted

r154 closed the `sub_82390880` caller-identification sub-thread on
cost-benefit grounds after `backtrace()` gave an answer contradicted by a
literal source-code check. This cycle picked up r154's own named
alternative: a real Ghidra static cross-reference scan of the retail XEX,
against `NtQueryInformationFile`'s real import-thunk address
(`0x823D031C`, read directly from this build's own
`generated/ppc_func_mapping.cpp:19520`, not inferred).

## What the static scan found

```
=== xrefs to 823D031C ===
  823908a4  UNCONDITIONAL_CALL  in Function_82390880@82390880  bl 0x823d031c
  total 1
```

Ghidra's reference manager reports exactly one cross-reference to the
import thunk, a direct `bl` at `0x823908A4`, inside `sub_82390880` — this
**independently agrees** with r146/r147's original static finding and with
this session's own literal-text grep of the generated C++ (exactly one
`NtQueryInformationFile(ctx` call site, inside `sub_82390880`). Two
independent static methods now agree.

## What live instrumentation found: this static answer is wrong for this run

An unconditional `fprintf` was placed as the first statement inside
`__imp__sub_82390880`'s C++ body (no `getenv` gate, to rule out an
environment-variable-detection failure as an explanation) and the resulting
binary was rebuilt and run through the same bounded probe that reliably
shows `NtQueryInformationFile` firing (confirmed via `AC6_NATIVE_IMPORT_TRACE`
in every prior cycle back to r150). The compiled call was verified present
in the actual linked binary before running:

```
$ objdump -d --disassemble=__imp__sub_82390880 ac6recomp | head
00000000000786e0 <__imp__sub_82390880>:
  ...
  786fc: lea 0xe2bf3e(%rip),%rsi   ; the [r155] format string
  78705: call 78110 <fprintf@plt>
```

The instrumented line unambiguously executes on every entry to this
function, with no conditional guard. **It did not print once** across a
full bounded probe run in which `NtQueryInformationFile` still fired
(confirmed via the same run's `AC6_NATIVE_IMPORT_TRACE` output). This is
not a diagnostic-placement mistake — the compiled object was inspected
directly — so the conclusion is unavoidable: **`sub_82390880` genuinely is
never entered in this run**, despite being the XEX's one and only
statically-referenced caller of the `NtQueryInformationFile` thunk address.

## Reconciling the contradiction

The two static methods (literal-text grep of generated C++, and Ghidra's
reference-manager xref lookup) and the one dynamic method (direct
instrumentation of the compiled function) cannot all be describing the same
call path. The most likely reconciliation, not yet directly verified: this
Ghidra project was analyzed `-noanalysis` for this scan (matching the
project's existing convention for cheap, targeted lookups) — a reference
manager built without full auto-analysis does not resolve computed/indirect
calls (e.g. a `bctrl` through a register loaded from a table of raw import
addresses), only references the loader and any prior analysis already
recorded. If some other guest code loads `0x823D031C` from a table and
branches to it indirectly, `NtQueryInformationFile` would execute via
XenonRecomp's own indirect-dispatch mechanism (a function-pointer lookup
into `ppc_func_mapping`) **without ever passing through `sub_82390880`'s
compiled body**, and such a reference would not appear in an
un-fully-analyzed reference manager. This is a plausible mechanism, not a
verified one — per this project's own discipline, it is recorded as
plausible-but-unconfirmed, not asserted as fact.

## Decision

This is a genuine confirmation of `CLAUDE.md`'s own "measure the instrument
before trusting it" principle, from the opposite direction of its usual
application: here, **two independent static methods that agreed with each
other were both wrong**, and only live, controlled instrumentation of the
actual compiled binary caught it. r154's cost-benefit closure of this
sub-thread stands and is now on firmer ground — not only is the specific
caller unidentified, but the two static techniques available in this
project (grep of generated C++, `-noanalysis` Ghidra xrefs) are now known
to both be insufficient for this particular question, and resolving it for
real would require a full Ghidra auto-analysis pass (a materially larger
investment than the "no oracle, no new lever" sub-thread has justified
since r147). This finding itself — not the original caller-ID question — is
the useful output of this cycle, and is exactly the kind of thing
`INSTRUMENT_DISCIPLINE.md` exists to index.

## Gates

`ctest` 9/9 (native profile) after final revert and clean rebuild. `git
status` unchanged from r154's own qualification (only pre-existing,
unrelated dirty state).

## Next

The `sub_82390880`/`NtQueryInformationFile` caller question remains closed
per r154, now with a documented reason two static techniques both failed on
it. Both of Gate 2's named frontiers remain blocked (DATA.TBL chain fully
traced per r144/r153; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked per r144).
Absent a new frontier surfacing, the next concrete option is a full Ghidra
`-analysis` pass (not `-noanalysis`) if the project judges the caller
question or similar indirect-call questions worth that investment; that
decision is left open rather than made unilaterally here, given no oracle
lever has depended on it since r147.
