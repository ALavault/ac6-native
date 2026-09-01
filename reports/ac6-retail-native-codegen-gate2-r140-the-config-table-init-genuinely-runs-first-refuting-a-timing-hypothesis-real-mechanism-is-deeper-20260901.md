# AC6 retail NTSC-U/J — the config-table init genuinely runs before the failing query, refuting a timing hypothesis; the real mechanism is inside `sub_82344058`'s BST lookup/insert logic (r140)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** Three
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R140_DIAG`) were added to gitignored, regenerated build-tree files
(`generated/ppc_recomp.38.cpp`, `.52.cpp`, `.54.cpp`), used to take live
measurements, and fully reverted via `cp` from `/tmp/*.orig` backups.
Confirmed reverted via `grep -c "r140"` returning 0 in all three files, a
clean `tools/build.py --target ntsc-uj --profile native` (`ctest` 9/9),
and 139/139 Python tests. `git status` on `recompilation/ace-combat-6-retail`
shows only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change.

## The hypothesis this cycle set out to test

r139 closed the causal chain down to "`sub_82338388(category=1,
setting=3, index=4)` legitimately returns `0`" and named understanding
*why* as next. Reading `sub_82344058` (the function that ultimately
computes this return value, reached via `sub_82339AA8`'s pool-pop
success path) in full shows it assigns a monotonically increasing "ID"
from a counter at `table + 80`, returning the counter's value *before*
incrementing it. Its sibling `sub_82344150(table)` initializes that same
counter to `1` (not `0`). This produced a clean, testable hypothesis: if
`sub_82344150(0x82910000)` -- confirmed via the same `lis`/`addi`
immediates as the table address every other cycle in this investigation
has used -- has not yet run by the time the failing query executes, the
counter would still hold its `.bss`-zero default, and the very first
successful lookup would return `0` instead of the expected `1`.

Grepping for `sub_82344150`'s only call site found it inside
`sub_82338848`, itself called from within `sub_821D5F48` (the same
retry-loop function this whole investigation has traced since r117) at
a point *textually before* the failing query's own call chain
(`sub_821CC288`, r139) -- consistent with the hypothesis, but text order
in one huge function is not execution order without a live check.

## The measurement that refutes it

A live diagnostic on `sub_82338848`'s entry, `sub_82344150`'s entry, and
`sub_82338388`'s return value, all in the same run, shows:

```
[r140] sub_82338848 ENTRY (config-table master init)
[r140] sub_82344150(table=0x82910000) CALLED -- sets [table+80]=1
[r140] sub_822834C0: sub_82338388 returned=0x00000000
```

**Both init calls genuinely execute, in the expected order, before the
failing query -- and the query still returns `0`.** This refutes the
"table never initialized" hypothesis outright: the counter this cycle
believed would still be `0` is, per `sub_82344150`'s own confirmed code,
set to `1` before the query ever runs.

## What this means: the mechanism is not initialization timing

Since `sub_82344058`'s counter path is confirmed initialized correctly,
the `0` this cycle measured must come from a **different path within
`sub_82344058`** than the fresh-counter-assignment one this cycle traced
first. Re-reading its earlier BST-search code (read partially in r139,
not fully accounted for in this cycle's hypothesis): the function first
walks a binary search tree keyed on a field from the descriptor
(`sub_823455D8`'s output), and only falls through to the counter-read
path (`loc_8234410C`) either when the tree is empty or after finding/
removing a matching node -- the exact branch taken depends on whether an
entry for this specific key **already exists** in the tree. If a prior
insert already created an entry for this key and stored an ID of `0`
into it *before* `sub_82344150` ever ran (impossible, since `0` could
only be assigned once, as the very first ID, and the trace above shows
init running before *this* query -- but does not rule out a *different*,
earlier query having already consumed and stored ID `0` for a *different*
key before the counter reached `1`) -- or if this cycle's read of which
value populates the returned `r30` at the "found existing" branch is
simply wrong -- either is plausible, and this cycle does not have enough
evidence to choose between them.

## Decision

No native code changed this cycle -- three rounds of temporary,
env-gated diagnostics, reverted and verified reverted (`ctest` 9/9,
139/139 Python, clean `git status`). This is a second consecutive
negative result in this specific sub-thread (following r138's), and it
is kept in the record for the same reason: a plausible, well-reasoned
static hypothesis was tested directly and refuted, narrowing where the
real mechanism can be without yet finding it. The BST found/not-found
branch inside `sub_82344058` -- not the counter's own initialization --
is now the concrete next target.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Add a live diagnostic *inside* `sub_82344058` itself (not just at its
   entry/exit) that distinguishes which branch is actually taken for the
   failing query -- the empty-tree/fresh-counter path (`loc_8234410C`
   reached directly) versus the found-existing-node path (reached via
   `loc_823440CC`/`loc_823440B4`) -- and, if the latter, dump the found
   node's own stored ID field directly rather than inferring it.
2. If an existing node with a stored ID of `0` is found, trace what
   earlier query created it and whether *that* query ran before or after
   `sub_82344150`'s counter reset -- this cycle's diagnostics did not
   count how many times `sub_82344058` itself runs across the whole
   probe, the same class of correlation gap r135/r138 already caught
   once each in this investigation.
3. Continue to test any hypothesis with a live measurement before
   trusting a static read, including ones that look as clean and
   well-reasoned as this cycle's own starting hypothesis did.
