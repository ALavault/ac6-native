# AC6 retail NTSC-U/J — `backtrace()` caller identification fails under apparent tail-call elision; the `sub_82390880`/`NtQueryInformationFile` caller sub-thread is closed on cost-benefit grounds (r154)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Two throwaway, env-gated diagnostics (`AC6_R154_DIAG`) tried in
sequence, each added to a gitignored, regenerated build-tree file
(`generated/ppc_recomp.66.cpp`, then `native-import-stubs.cpp`), each fully
reverted via `cp` from a `/tmp/*.orig` backup and confirmed via `grep -c
"r154"` returning 0. Clean `tools/build.py --target ntsc-uj --profile
native` (`ctest` 9/9) after the final revert. `git status` unchanged from
r153's own qualification.

## What was attempted

r150's open item (identify which caller reaches `sub_82390880` on the
`DATA.TBL` handle) was picked up. The first diagnostic instrumented
`sub_82390880`'s own entry (the function r146/r147 identified as the sole
static call site for `NtQueryInformationFile`/`NtSetInformationFile`) to
fire on every call. **It never fired once**, despite the same run's import
trace showing both `NtQueryInformationFile` and `NtSetInformationFile`
firing right after each `DATA.TBL` open, exactly as in r150.

## What was found: the literal call site is not what actually runs

Moving the diagnostic directly into `__imp__NtQueryInformationFile` itself
(`native-import-stubs.cpp`) and capturing a `backtrace()` there resolves,
via `addr2line`, to:

```
__imp__NtQueryInformationFile
sub_821F5630
sub_82351550
sub_82345B28
__imp__sub_82343E18
__imp__sub_823453E8
sub_821F8008
```

**Not `sub_82390880` anywhere in this chain.** Reading `sub_821F5630`'s full
body (`ppc_recomp.27.cpp:2746`-2813, 68 lines) shows its *only* call is to
`sub_821F75B8` — not to `NtQueryInformationFile`, and not to `sub_82390880`.
Reading `sub_821F75B8` in turn (`ppc_recomp.27.cpp:8287`-8324, 37 lines)
shows its only call is to a small RTL helper (`0x823d018c`) — again neither
target. **Neither function in the reported backtrace literally calls the
next frame down**, which only has one explanation consistent with this
project's own build settings (`-O3`): the compiler performed sibling/tail
call optimization on one or more of the real intermediate frames, eliding
them from the host call stack. `backtrace()` under `-O3` shows *the nearest
surviving return address*, not the true guest call graph, whenever a tail
call chain is involved — it happened to work for r152's chain (verified
independently: every backtrace frame there was cross-checked against a
literal source-level `bl`/call, and all matched), but nothing guarantees
that in general, and here it clearly does not.

## Methodology note for future cycles

`backtrace()`+`addr2line` is not, by itself, reliable evidence of a caller
in this codebase's `-O3` build — it must be cross-verified against a literal
call in the named function's source (as r152 did, checking each frame's body
for the actual `bl`/call instruction), not trusted on its own. When that
cross-check fails (as here), the backtrace names are not evidence of the
real call chain and should be discarded rather than reported as a finding.
This is worth keeping in `INSTRUMENT_DISCIPLINE.md`-style institutional
memory the next time a caller-identification question comes up.

## Decision: close this sub-thread on cost-benefit grounds

Re-deriving the true call path (which would need either a Ghidra static
cross-reference scan of the real XEX for the `NtQueryInformationFile` import
thunk address, or a `ctx.lr`-based technique if a scoped `PPC_CONFIG_SKIP_LR`
build were available) is real, boundable work — but r147 already
characterized this whole idiom (`NtQueryInformationFile`/
`NtSetInformationFile` as a truncate-to-zero pair) as a debug movie-capture
feature unrelated to `DATA.TBL`'s actual load failure, and r150-r153 have
since fully closed the DATA.TBL causal chain by an entirely separate route
(the `sub_821CC288`→`sub_82222D80` allocation-size chain) that does not
depend on this question at all. Per r144's own cost-benefit standard for
this exact sub-thread, further chasing *which* function calls
`NtQueryInformationFile` on the `DATA.TBL` handle has no remaining leverage
on the tracked crash or on any other open native-runtime question — it is
closed here, not solved, and that is recorded plainly rather than left as a
silently-abandoned lead.

## Where this leaves Gate 2's two named frontiers

With this sub-thread closed, both of this investigation's long-standing
named frontiers are again in the same state r144 found them in: the
DATA.TBL crash chain is fully traced (mechanism understood end-to-end,
no further native-runtime lever, per r144 and now r153) and
`IM_LOAD_IMMEDIATE`→SPIR-V remains explicitly policy-blocked (no oracle,
per `CLAUDE.md` and the shader translator's own fail-closed refusal, r144).
Three genuine native-runtime bugs were still found and fixed along the way
this session (r145, r148, r149), on their own merits, independent of
whether they resolved the tracked crash — the same standard r130-r131's
original fix set.

## Gates

`ctest` 9/9 (native profile) after final revert and clean rebuild. `git
status` unchanged from r153's own qualification (only pre-existing,
unrelated dirty state).

## Next

Per r144's own precedent when both named frontiers were last found blocked:
run the project's read-only maintenance audits
(`audit_claude_md_numbers.py`, `audit_contract_derivations.py`,
`audit_ac6_contract_addresses.py`, `audit_instrument_discipline_index.py`,
`audit_ac6_contract_artifacts.py`) as a sanity check, and treat any further
Gate 2 progress as contingent on either a new frontier surfacing from that
scan or a deliberate decision to invest in the Ghidra-based static
cross-reference work this cycle judged not yet justified.
