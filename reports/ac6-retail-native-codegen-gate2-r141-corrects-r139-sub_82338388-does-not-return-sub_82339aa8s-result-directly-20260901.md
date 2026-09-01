# AC6 retail NTSC-U/J — corrects r139: `sub_82338388` does not return `sub_82339AA8`'s result directly; a post-processing step (`sub_821F4128`/`sub_821F7538`) between them produces the observed `0` (r141)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** Three
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R141_DIAG`) were added to gitignored, regenerated build-tree files
(`generated/ppc_recomp.38.cpp`, `.52.cpp`, `.54.cpp`), used to take live
measurements, and fully reverted via `cp` from `/tmp/*.orig` backups.
Confirmed reverted via `grep -c "r141"` returning 0 in all three files, a
clean `tools/build.py --target ntsc-uj --profile native` (`ctest` 9/9),
and 139/139 Python tests. `git status` on `recompilation/ace-combat-6-retail`
shows only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change.

## Following r140's own instruction

r140 named "instrument inside `sub_82344058` to distinguish its branches
for the failing query" as next. This cycle did exactly that -- and the
result overturns a premise r139 asserted without direct verification.

## The measurement that breaks r139's chain

A live diagnostic on `sub_82344058`'s entry/exit shows it is called
**five times** in the whole run and **never returns `0`** -- it returns
`1, 2, 3, 4, 5` in strict sequence, matching r138's five
`sub_82343F20` pool-pop calls exactly. This alone contradicts r139's
claim that `sub_82338388`'s `0` return traces through
`sub_82339AA8 -> sub_82344058`.

Adding entry/return tracing to `sub_82339AA8` itself and correlating all
three diagnostics (`sub_82339AA8`, `sub_82344058`, and
`sub_822834C0`'s own print of `sub_82338388`'s return) in one combined
run resolves it precisely:

```
[r141] sub_82344058 ENTRY ... [table+80]=1  -> RETURN=1   (unrelated caller)
[r141] sub_82339AA8 ENTRY obj=0x8feffa90 cat=1 setting=3 p3=4 p4=0   <- OUR QUERY
[r141] sub_82344058 ENTRY ... [table+80]=2  -> RETURN=2
[r141] sub_82339AA8 RETURN=0x00000002                                <- succeeds, returns 2
[r141] sub_822834C0: sub_82338388 returned=0x00000000                <- but this prints 0
```

**`sub_82339AA8`, for the exact query this whole investigation has
traced since r137 (`category=1, setting=3, index=4, flags=0`), returns
`2` -- a real, positive, successful result -- not `0`.** Yet
`sub_822834C0`'s own diagnostic, printed immediately after its call into
`sub_82338388`, shows `0`. **r139's premise that `sub_82338388` returns
`sub_82339AA8`'s result directly is wrong**, corrected here per this
project's own discipline of correcting even its immediately preceding
cycles.

## Where the transformation actually happens

Re-reading `sub_82338388`'s success branch (read partially in earlier
cycles, its full control flow not previously traced) shows it does not
simply forward `sub_82339AA8`'s return value. On success
(`sub_82339AA8`'s result `>= 0`), it instead:

```cpp
r4 = -1;
r3 = [r1 + 80];              // a LOCAL stack buffer sub_82339AA8 wrote into
sub_821F4128(r3, r4);        // called on that buffer, not on the integer result
r11 = [r1 + 88];              // a DIFFERENT stack local, 64-bit
r31 = extsw(r11);             // sign-extend its low 32 bits -- the REAL return
```

`sub_82339AA8`'s integer return (`2`, an internal sequence ID from
`sub_82344058`) is discarded on this path; the actual value `sub_82338388`
returns comes from a **separate output buffer** `sub_82339AA8` populated
during the call (passed as one of its own arguments, `r9`/`ctx.r9`), read
back through `sub_821F4128`. `sub_821F4128(buffer, -1)` is a two-line
tail call into `sub_821F7538(buffer, -1, 0)`, unread this cycle.

This reframes the whole open question from r139-r140: the `0` is not
necessarily "a count that happens to be zero" at all -- it may be one of
the *query's own input parameters* (`p4 = 0` in the trace above) being
read back through this output buffer, in which case a `0` result would
be entirely expected and correct, not a sign of an empty/uninitialized
resource. That is not yet established; it is the leading hypothesis this
cycle's evidence points to, named explicitly as such rather than
asserted.

## Decision

No native code changed this cycle -- three rounds of temporary,
env-gated diagnostics, reverted and verified reverted (`ctest` 9/9,
139/139 Python, clean `git status`). This corrects a load-bearing premise
in r139's own report (not merely narrowing between two possibilities, as
r138/r140 did) -- `sub_82338388`'s return does not come from
`sub_82339AA8`'s integer result, it comes from a distinct output buffer
processed by `sub_821F4128`/`sub_821F7538`. The investigation's ten-step
causal chain from r139 (crash back to "a query returns 0") remains
correct in every link *except* the specific claim about which value
becomes that `0` and why -- that link is now known to be wrong in its
mechanism, though not in its observable endpoint (the `0` itself, and
everything downstream of it, are unaffected and still accurate).

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Read `sub_821F7538` (the real body behind `sub_821F4128`'s tail
   call) to find what it does with the output buffer `sub_82339AA8`
   populated -- this is the actual site that produces `sub_82338388`'s
   final `0`.
2. Identify what `sub_82339AA8` writes into its own `r9`/output-buffer
   argument (the local at the caller's `r1+80`) during a successful
   query -- trace this back to `sub_823455D8`'s field layout (already
   read in r139) to see whether the eventual `0` is literally one of the
   query's own input parameters (`p4=0`) or something else the pool/BST
   machinery computes.
3. If it is indeed an echoed input parameter, re-examine whether
   `sub_821CC288`'s use of this whole subsystem's result as an
   allocation size was ever a reasonable design at all, versus
   `sub_821CC288` calling the wrong query/helper for what it actually
   needs -- a different class of question than "why does a count read as
   zero."
4. Keep testing each hypothesis with a live, correlated measurement
   before extending it into a new report's premises -- this cycle's own
   correction of r139 is exactly the kind of check r135/r138 already
   demonstrated is necessary and cheap to run.
