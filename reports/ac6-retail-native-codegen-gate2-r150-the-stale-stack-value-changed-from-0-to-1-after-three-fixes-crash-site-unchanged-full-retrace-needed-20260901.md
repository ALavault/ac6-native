# AC6 retail NTSC-U/J — the r139/r142 stale-stack value changed from `0` to `1` after this session's three fixes; DATA.TBL's own handle now flows through the file-truncate idiom; crash site still unchanged (r150)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle -- one throwaway,
env-gated diagnostic added to a gitignored, regenerated build-tree file
(`generated/ppc_recomp.38.cpp`), used to take a single live measurement,
and fully reverted via `cp` from a `/tmp/*.orig` backup. Confirmed
reverted via `grep -c "r150"` returning 0, a clean `tools/build.py
--target ntsc-uj --profile native` (`ctest` 9/9), and 142/142 Python
tests. `git status` shows only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change.

## Why this check was worth running

r149 named "check whether this fix changes observable behavior" as
standing methodology after any real native-runtime fix. This session has
now landed three such fixes (r145, r148, r149), each changing real
guest scheduling or execution flow. r142 established that
`sub_82338388`'s ultimate "return value" is a read of stale,
never-written stack memory (`[caller_frame+88]`) -- a value whose
content is entirely a function of what earlier, unrelated code happened
to leave on that exact stack slot. Given three fixes this session have
already changed which code runs and in what order, that stale value was
worth re-measuring directly rather than assuming r139-r142's
characterization (a hardcoded, effectively-fixed `0`) still holds.

## What was found: the value changed

A single, bounded, live diagnostic on `sub_822834C0`'s call into
`sub_82338388` (the same call site r139/r141/r142 all measured) shows:

```
[r150] sub_822834C0: sub_82338388 returned=0x00000001 (signed=1)
```

**Not `0` -- `1`.** This directly confirms r142's own characterization
of the mechanism (uninitialized stack content, sensitive to prior
execution history) rather than contradicting it: the *value* was never
fixed at `0` by anything in the traced code, it was whatever the stack
happened to hold, and this session's fixes changed what the stack holds.

## A further, more surprising change: DATA.TBL's own handle now reaches the file-truncate idiom

The same run's full import trace shows, immediately after `DATA.TBL`
opens successfully:

```
[NtCreateFile] "DATA.TBL" -> ok
[r150] sub_822834C0: sub_82338388 returned=0x00000001 (signed=1)
[offline-import] NtQueryInformationFile
[RtlNtStatusToDosError] unmapped status=0xc00000bb
[NtCreateFile] "DATA.TBL" -> ok
[offline-import] NtSetInformationFile
[RtlNtStatusToDosError] unmapped status=0xc00000bb
[NtCreateFile] "DATA00.PAC" -> ok
[NtCreateFile] "DATA01.PAC" -> ok
```

`NtQueryInformationFile`/`NtSetInformationFile` have exactly one static
call site each (r146), inside `sub_82390880`, itself called from exactly
two functions (`sub_821E9F50`/`sub_821EA2F8`) r147 traced to a debug
movie-capture feature via a nearby `sprintf`/log-string pair. **This run
shows that same code executing on a handle that opens `DATA.TBL`**,
which is either evidence that `sub_821E9F50`/`sub_821EA2F8` are more
general "prepare a freshly-opened file" utilities than r147's narrower
reading concluded (only one of possibly several logical callers was
actually traced to the movie-capture string), or evidence of a genuinely
new code path this session's fixes newly expose. r147's conclusion is
not yet contradicted by direct evidence -- no string reference has been
checked for *this* specific invocation -- but it is no longer the whole
story, and should not be treated as fully settled without re-checking
which caller reaches `sub_82390880` on this particular handle.

## The crash site is unchanged

Despite both of the above, the `sub_821F7C80` crash still reproduces at
the identical instruction via the identical call chain (`gdb` backtrace
confirmed, matching every prior cycle since r131). Whether this is
because the corrupted-notification-list mechanism (r132/r133) is still
reached via the exact same route, or via a *different* route that
happens to land on the same crash site, is not established by this
cycle's one measurement.

## Decision

This is recorded as a significant, confirmed change to this
investigation's own prior measurements -- not a contradiction of them,
but a reminder that the DATA.TBL causal chain's intermediate values
(established across r130-r142) were measured against a binary that no
longer matches this session's own committed state. Fully retracing that
chain from `sub_821CC288` onward against the *current* binary is a
substantial undertaking, comparable in scope to r130-r142's own arc, and
is explicitly not attempted in this cycle -- doing so hastily risks
re-deriving stale conclusions against a moving target. This is named as
a large, well-scoped next investigation rather than rushed.

## Gates

No native code changed this cycle; `ctest` 9/9, Python 142/142, `git
status` clean (only the pre-existing, unrelated `upstream/AC6_recomp`
submodule pointer change).

## Next

1. Before any further deep tracing, re-run the full r130-r142 causal
   chain's own key measurements (r135's allocation size, r137's garbage
   value, r139's category/setting query, r141/r142's stack-content
   trace) against the *current* binary (post r145/r148/r149) in one
   consolidated pass, rather than assuming any single one still holds in
   isolation -- this cycle only re-checked the single final link.
2. Determine which specific caller of `sub_82390880` reaches it on the
   `DATA.TBL` handle in this run, and whether r147's movie-capture
   identification still describes the *other* call site correctly or
   needs its own re-check.
3. Given the scope, treat this as a fresh multi-cycle investigation
   thread rather than a single-cycle fix -- the same discipline this
   whole DATA.TBL arc has followed throughout (r130-r142) applies to
   re-tracing it against a changed binary just as much as it did to
   tracing it the first time.
