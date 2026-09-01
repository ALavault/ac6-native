# AC6 retail NTSC-U/J — the `0` is a read of stale, never-written stack memory, not a real returned value; `sub_82338388`'s wait times out and its own "result" was never populated (r142)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** Two
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R142_DIAG`) were added to gitignored, regenerated build-tree files
(`generated/ppc_recomp.27.cpp`, `.52.cpp`), used to take live
measurements, and fully reverted via `cp` from `/tmp/*.orig` backups
(the first round's own log exceeded 300MB and was discarded without
being read in full -- an instrumentation placement mistake, corrected
within the same cycle before drawing any conclusion from it). Confirmed
reverted via `grep -c "r142"` returning 0 in both files, a clean
`tools/build.py --target ntsc-uj --profile native` (`ctest` 9/9), and
139/139 Python tests. `git status` on `recompilation/ace-combat-6-retail`
shows only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change.

## Following r141's own instruction

r141 named reading `sub_821F7538` (the real body behind `sub_821F4128`'s
tail call) as next. It is a real kernel wait loop:
`NtWaitForSingleObjectEx(handle, alertable=1, timeout)`, retrying while
the status is `STATUS_PENDING`-shaped, returning `-1` only on a genuinely
negative (error) status, and otherwise returning the wait's own raw
status code directly.

An unthrottled diagnostic on every call into this wait produced over
300MB of output before the process crashed -- this function is a hot,
frequently-hit polling primitive across the whole subsystem, not the
rare, DATA.TBL-specific call this cycle initially assumed. That output
was discarded unread (per this project's own discipline: an
instrument that floods rather than measures is not trusted at face
value), and the diagnostic was narrowed to the one call site this
investigation actually needs: `sub_82338388`'s own use of the wait's
result.

## The measurement that resolves it

`sub_82338388`'s success branch does not use the wait's return value at
all. Its actual code:

```cpp
r3 = [r1 + 80];         // handle
sub_821F4128(r3, -1);   // the wait; its own return (ctx.r3) is discarded here
r11 = load_u64(r1 + 88); // a DIFFERENT stack slot, never written by anything
                          // in this call chain
r31 = sign_extend_32(r11);  // <- sub_82338388's real return value
```

A live diagnostic bracketing this exact call, printing the handle, the
pre-existing (never-yet-written) content of `[r1+88]`, and the wait's
actual return status, shows:

```
[r142] sub_82338388 pre-wait: handle=0x0000012e pre-existing[stack+88..95]=0x0000000000000000
[r142] sub_82338388 post-wait: sub_821F4128 returned=0x00000102 [stack+88..95]=0x0000000000000000
[r142] sub_82338388 pre-wait: handle=0x00000131 pre-existing[stack+88..95]=0x00000000000002d0
[r142] sub_82338388 post-wait: sub_821F4128 returned=0x00000102 [stack+88..95]=0x00000000000002d0
```

Two things are now certain, from direct measurement rather than static
inference:

1. **The wait actually times out** (`0x102 = STATUS_TIMEOUT`), not
   succeeds -- contradicting this cycle's own initial reading of
   `sub_821F7538`'s control flow (a positive, non-negative status like
   `STATUS_TIMEOUT` does not hit the "negative -> return -1" branch, so
   the function falls through and returns the raw `0x102` -- this was
   read correctly, but this cycle initially assumed `0x102` was
   `sub_82338388`'s eventual value; it is not).
2. **`[r1+88]` is never written by anything in this call, by our own
   HLE's `NtWaitForSingleObjectEx`, or by any function this cycle traced
   -- its value before the call and after the call are identical in both
   observed cases.** `sub_82338388`'s return is a sign-extended read of
   whatever was already sitting in that stack memory before this
   function ever ran -- `0` in the first (our target) case, `0x2d0`
   (720) in the second.

This is the true, final origin of the `0` r139 measured: it is not a
count, not a config value, not a wait-completion status, and not
anything `sub_82338388` computed -- it is **stale stack content left
over from an earlier, unrelated call that happened to occupy the same
stack memory**, read as if it were meaningful output.

## What this means for the rest of the chain

Given the shape (`[r1+80]` used as a handle/status slot, `[r1+88]`
directly following it, 8-byte aligned, read as a 64-bit value then
truncated to 32 bits), this strongly resembles a Windows
`IO_STATUS_BLOCK`-style local (`{Status; Information;}`) that some
asynchronous-completion convention would normally populate -- via an APC,
a real I/O completion callback, or a kernel mechanism this project's
synchronous HLE stubs do not model. That is a plausible explanation for
*why* real hardware might reliably see a real value here while this
recompilation sees leftover stack garbage, but it is not established;
nothing in the traced call chain (`sub_82339AA8`, `sub_823455D8`,
`sub_821F4128`, `sub_821F7538`) writes to this slot, so the writer, if
one exists at all, is still unlocated.

## Decision

No native code changed this cycle -- two rounds of temporary, env-gated
diagnostics (the first discarded unread for being disproportionately
hot, the second properly scoped), reverted and verified reverted
(`ctest` 9/9, 139/139 Python, clean `git status`). This closes r141's
own open question precisely: the `0` is confirmed to be a read of never-
written stack memory, not a value any function in this chain computed or
returned. Per this project's discipline against guessing, whether real
hardware's compiler/ABI would leave genuinely different (non-zero, non-
garbage) data in that exact stack slot -- making this an artifact of this
recompilation's own stack layout rather than the retail game's design --
is named as the next, and possibly final, question in this specific
sub-thread, rather than asserted.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Search for any writer to `sub_82338388`'s own `[r1+88]` slot that
   this cycle has not yet traced -- in particular, whether
   `NtWaitForSingleObjectEx`'s real signature or calling convention on
   this XEX passes a hidden pointer to this exact stack region (an
   `IO_STATUS_BLOCK`-shaped output parameter this project's generic HLE
   stub does not populate), by checking whether any *other* caller of
   the same kernel import anywhere in the codebase writes to an
   equivalent stack offset after the call.
2. If no writer exists anywhere in the retail binary, this specific
   value is uninitialized-stack-read behavior in the *original* retail
   code -- not something this recompilation introduced -- and the
   question shifts to whether this project's stack-frame emulation
   happens to produce different leftover content than real Xbox 360
   hardware would at this exact point (a genuine, if narrow, semantic
   gap in this project's ABI/stack modeling), which would need
   comparing this project's actual host stack-allocation pattern against
   the real PPC ABI's stack reuse behavior.
3. Given the multi-cycle depth already reached (r129 through r142) and
   the genuinely obscure, narrow nature of this specific leaf finding, a
   cost/benefit check is warranted before continuing further into this
   exact sub-thread -- consider whether the DATA.TBL crash chain's value
   as a demonstrated, fully-traced case study (r130-r142, already an
   unusually complete example of this project's evidence discipline) now
   exceeds the value of resolving this one remaining stack-content
   question, versus surveying whether other Gate 2 frontiers exist that
   would make better use of the next several cycles.
