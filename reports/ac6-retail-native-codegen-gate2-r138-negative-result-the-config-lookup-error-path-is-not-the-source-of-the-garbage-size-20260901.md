# AC6 retail NTSC-U/J — negative result: the config-lookup error path this cycle initially suspected is not the source of the garbage allocation size; the real source remains unidentified (r138)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** Two
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R138_DIAG`) were added to a gitignored, regenerated build-tree file
(`generated/ppc_recomp.54.cpp`), used to take live measurements, and
fully reverted via `cp` from a `/tmp/*.orig` backup. Confirmed reverted
via `grep -c "r138"` returning 0, a clean `tools/build.py --target
ntsc-uj --profile native` (`ctest` 9/9), and 139/139 Python tests. `git
status` on `recompilation/ace-combat-6-retail` shows only the
pre-existing, unrelated `upstream/AC6_recomp` submodule pointer change.

## The hypothesis this cycle set out to test

r137 named `sub_82283728` and `sub_822834C0`'s three callees as the next
thing to read in full, without assuming their semantics. Reading that
chain statically found a plausible, well-shaped story: `sub_822834C0`
calls `sub_82338388(buffer, 1, 3, 4, 0)`, which structurally resembles an
`ExGetXConfigSetting`-style call (category/setting bounds checks
`category<=5`, `setting<5` match), which in turn calls `sub_82339AA8`.
That function validates its category/setting arguments, then calls
`sub_82343F20(0x82910000)` -- a fixed "provider table" object -- and, if
that call returns `0`, returns a hardcoded error constant computed as
`(-16842752) | 65528 = 0xFEFFFFF8`. This is one bit off from r137's
measured garbage size (`0xFEFFFFF9`), close enough to look like the
source with an off-by-one from sign extension or rounding elsewhere in
the chain -- a strong-looking lead worth testing directly rather than
trusting the static read alone.

## The measurement that refutes it

`sub_82343F20`'s own body, read in full this cycle, is not a simple
"is a provider registered" boolean check as first assumed from its early
lines -- it is a **pool-pop operation**: if a count field at `object+1388`
is nonzero, it unlinks the head of a doubly-linked free list and returns
a field from the popped node (`[popped_node + 8]`); only an empty pool
(`count == 0`) returns `0`.

A live diagnostic on both the pool-count check and the function's actual
return value, across every call in one run, shows:

```
[r138] sub_82343F20(table=0x82910000): [table+1388]=16 ...
[r138] sub_82343F20 returns r30=0x82910054
[r138] sub_82343F20(table=0x82910000): [table+1388]=16 ...
[r138] sub_82343F20 returns r30=0x82910094
[r138] sub_82343F20(table=0x82910000): [table+1388]=15 ...
[r138] sub_82343F20 returns r30=0x829100d4
[r138] sub_82343F20(table=0x82910000): [table+1388]=14 ...
[r138] sub_82343F20 returns r30=0x82910114
[r138] sub_82343F20(table=0x82910000): [table+1388]=13 ...
[r138] sub_82343F20 returns r30=0x82910154
```

**Five calls, five successes** -- the pool count decrements normally
(16, 16, 15, 14, 13) and every call returns a real, valid, sequentially-
spaced address (stride `0x40`), never `0`. `sub_82339AA8`'s error branch
(the one producing `0xFEFFFFF8`) depends on this call returning `0`; it
never does in this run. **This specific error path is not the source of
the garbage size r137 measured.** The static read that motivated this
hypothesis was plausible and worth testing, but wrong -- exactly the kind
of plausible-but-uncontrolled rule this project's discipline refuses to
accept without a live check, and the live check refutes it.

## What is and is not established after this cycle

Established: the garbage size (`0xFEFFFFF9`, r137) does not come from
`sub_82339AA8`'s `sub_82343F20`-failure branch, because that branch is
never taken in this run. Not established: whether `sub_82338388` /
`sub_822834C0` / `sub_821CC288`'s failing (third, per r137) call even
reaches `sub_82339AA8` and `sub_82343F20` at all -- this cycle's
diagnostic counted all calls to `sub_82343F20` in the whole run without
correlating any specific one to r137's specific failing `sub_82222D80`
call. That correlation gap is exactly the kind of mistake r135 caught and
fixed within its own cycle (assuming a piece of code belongs to a
call path without verifying it); it is surfaced here explicitly rather
than repeated silently.

## Decision

No native code changed this cycle -- two rounds of temporary, env-gated
diagnostics, reverted and verified reverted (`ctest` 9/9, 139/139
Python, clean `git status`). This cycle is a negative result: a specific,
plausible-looking hypothesis was tested directly and refuted. That is
kept in the record rather than discarded, so a future cycle does not
re-derive and re-test the same wrong lead. The actual source of the
garbage size is still open.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Before reading further code, establish the actual call chain for
   r137's specific failing `sub_82222D80` call (the third of four) with
   the same call-counting/entry-tagging technique r135 used to catch its
   own attribution mistake -- confirm or refute whether
   `sub_821CC288 -> sub_82283728 -> sub_82338388 -> sub_82339AA8 ->
   sub_82343F20` is even the right chain for *that specific* call, rather
   than one of several unrelated call sites sharing the same functions.
2. If it is the right chain, `sub_82339AA8` still calls
   `sub_823455D8` on its success path (unread this cycle) -- that is
   where a real config value would come from, and where a different kind
   of failure (a valid-looking but wrong value, rather than the hardcoded
   error constant) could originate.
3. If it is not the right chain, back up to `sub_821CC288`'s own call
   site and trace which of `sub_82283530`/`sub_822836A8` (called inside
   `sub_82283728`, unread this cycle) actually produces the value that
   flows into `sub_822834C0`.
4. Keep testing plausible-looking static reads with a live measurement
   before trusting them, per this cycle's own result.
