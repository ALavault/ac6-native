# AC6 retail NTSC-U/J — GATE2's crash chain (r100-r107) is closed: `MmQueryStatistics` never wrote its output buffer (r108)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed for real this cycle (not reverted):
`tools/materialize_native_import_stubs.py` gained a `MmQueryStatistics`
case; `tests/test_materialize_native_import_stubs.py` gained a matching
test. One temporary diagnostic (`AC6_R108_DIAG`, gated `fprintf`) was
added to and fully reverted from the generated (gitignored, untracked)
`ppc_recomp.23.cpp` before this report — confirmed via
`grep -c "r108"` returning 0 in that file and `ctest` 9/9 passing
afterward.

## Continuing r105-r107's frontier

r105 found GATE2 (`sub_821F4078`, wrapping `MmAllocatePhysicalMemoryEx`)
fails from a signed-arithmetic underflow: `r11 - 0x800000` where
`r11 = 0x400000`, giving a ~4.09GiB request. r106 traced `r11`'s origin
to guest address `0x8feffd1c` (`Function_821D5F48`'s frame, offset 108)
and found nothing in the traced PPC call graph — up to and including
`_xstart`'s own frame — writes it, calling it "uninitialized." r107 ruled
out the XEX's declared stack size as an explanation and left two open
directions: trace a fuller kernel/loader boot sequence, or scope this as
a probe-harness limitation.

**Both were the wrong frame.** r106's stack-frame search looked for
stores keyed off `ctx.r1` in the caller's own frame and its ancestors.
It missed a frame-pointer escape: the caller doesn't own that memory by
writing it directly — it hands a pointer *into* its own frame to a
callee, which writes through that pointer instead. A base-register grep
over the caller's instructions cannot see a write made through a
different register in a different function.

## What the disassembly actually shows

`Function_821D5F48`, `generated/ppc_recomp.23.cpp` line 18875 (`sub_82331E78`
is `_xstart`'s own housekeeping call, unrelated to this thread):

```
// addi r3,r1,96
ctx.r3.s64 = ctx.r1.s64 + 96;
// bl 0x821f4820
sub_821F4820(ctx, base);
// lwz r11,108(r1)
r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 108);
```

`r1+96` is passed as `sub_821F4820`'s sole argument, and the very next
read is `r1+108` -- offset 96+12. `sub_821F4820`'s own generated body
(`generated/ppc_recomp.27.cpp` lines 267-335) resolves the callee:

```
// mr r31,r3            <- r31 = the caller's out-buffer pointer (r1+96)
// addi r3,r1,80
ctx.r3.s64 = ctx.r1.s64 + 80;   // its own local scratch buffer
// stw r11,80(r1)        <- writes 104 (the scratch buffer's declared Length)
// bl 0x823d01bc
__imp__MmQueryStatistics(ctx, base);
...
// lwz r8,92(r1)
ctx.r8.u64 = PPC_LOAD_U32(ctx.r1.u32 + 92);
...
// rlwinm r8,r8,12,0,19      (pages -> bytes, shift left 12, i.e. *4096)
ctx.r8.u64 = __builtin_rotateleft64(...) & 0xFFFFF000;
...
// stw r8,12(r31)
PPC_STORE_U32(r31.u32 + 12, ctx.r8.u32);
```

`r31 + 12` is exactly `Function_821D5F48`'s `r1 + 96 + 12 = r1 + 108` --
the address r106 pinpointed. So the slot *is* written, by
`sub_821F4820`, as a direct function of what `__imp__MmQueryStatistics`
leaves at its own scratch offset +12 (`r1_inner + 80 + 12 = r1_inner +
92`, the `lwz r8,92(r1)` above).

## The actual root cause: a default HLE stub that never wrote guest memory

`__imp__MmQueryStatistics` is materialized by
`tools/materialize_native_import_stubs.py`. Before this cycle it had no
dedicated case and fell through to the file's generic default
(`render_body`, previously ending at line 398):

```python
    return f"""  trace_offline_import("{name}");
  ctx.r3.u64 = kOfflineStatus;
"""
```

This sets only `ctx.r3` (a status code) and never writes through the
buffer pointer the caller passed in `ctx.r3` on entry. Confirmed this is
what actually built and ran: `grep -n "MmQueryStatistics"
build/.../codegen-20260831-mapfix-96838/native-import-stubs.cpp` showed
exactly this default body, generated fresh by `tools/build.py` from the
XEX's real import table at every build.

So the "uninitialized" 32-bit value r105/r106 found was never kernel- or
loader-territory at all: it's whatever stale bytes already occupied
`sub_821F4820`'s own stack-allocated scratch buffer, propagated through
one `rlwinm` pages-to-bytes transform and one store, because the stub
that was supposed to fill that buffer does nothing to guest memory.

## Fix

Added a `MmQueryStatistics` case to `render_body()` writing the two
fields `sub_821F4820` actually reads (`+4`, consumed as a
`TotalPhysicalPages`-shaped field; `+12`, the field this whole chain
traces) with values derived from the Xbox 360's well-documented unified
512MiB (`0x20000000`-byte) physical memory, at the 4KiB page granularity
the caller's own `rlwinm`-by-12 already establishes (`0x20000` total
pages; `0x18000` = 384MiB reported available, leaving headroom for an
unmodeled OS/kernel reservation without asserting its exact real
figure). The two fields nothing in the traced call graph reads (`+16`,
`+20`) are zeroed rather than guessed at -- this stub does not invent
values for anything unread, matching the discipline already applied to
`+4`/`+12`. `ctx.r3` is set to `0` (`STATUS_SUCCESS`), matching every
other implemented stub's convention; `sub_821F4820`'s caller does not
check `MmQueryStatistics`'s own return value regardless.

Added `test_query_statistics_fills_pages_the_caller_reads` to
`tests/test_materialize_native_import_stubs.py`, asserting the two
stores and the chosen `0x18000` constant appear in the materialized
output. Full suite: 18/18 (was 17/17 before this cycle's addition).

## Verified live, not just by reasoning about the disassembly

Rebuilt via `tools/build.py --target ntsc-uj --profile native`
(regenerates `native-import-stubs.cpp` from the fixed script, then
recompiles `ac6recomp`). Temporarily instrumented `ppc_recomp.23.cpp`
(gated `AC6_R108_DIAG`, reverted before this report) at the same two
points r105/r106 used -- the `r1+108` read and the post-`sub_821F4078`
result:

```
[r108] gate2 field+12=0x18000000
[r108] gate2 alloc result r3=0x16f70000
```

`0x18000000` = `0x18000 * 4096` exactly -- the new stub value reaching
this exact slot, live, end to end. `r30 = 0x18000000 - 0x800000 =
0x17800000` (positive; no more underflow), and `sub_821F4078`'s
allocation now **succeeds** (`r3 = 0x16f70000`, nonzero) instead of
failing as it did in r105 (`r3` observed 0 there, taking the shared
bailout branch every prior cycle traced). GATE2 no longer fails.

Reverted the diagnostic, rebuilt clean, reran the same bounded entry
probe (`AC6_NATIVE_ALLOW_ENTRY_PROBE=1`, `SDL_AUDIODRIVER=dummy`,
`SDL_VIDEODRIVER=dummy`, 30s bound) with no instrumentation:

```
ac6recomp: native runtime booted media "..."
ac6recomp: mapped XEX image 0xa98000 bytes at 0x82000000
ac6recomp: populated 19832 generated indirect-call mappings
ac6recomp: XEX entry 0x821f5ed0 resolves to generated function 0x821f5ed0
ac6recomp: generated guest ABI smoke call passed
```

Process hit the 30s bound (`exit 124`) with **no crash** -- a strict
improvement over every prior cycle back through r100, which all ended in
a SIGSEGV inside `sub_821D6C20` once the predicate-decode fix (r100)
first reached that far. A stale core dump was present in
`/var/lib/apport/coredump/` (mtime ~02:10, well before this run's
~05:50 wall clock) and was confirmed *not* to be from this run before
concluding no crash occurred.

## Decision

This is a real fix, not a diagnostic reversion: it corrects r106's own
conclusion (which this report names explicitly, per this project's
correct-predecessors-by-name discipline) that the value's owner falls
outside every traced stack frame. It does not; the owner is a specific,
identified, previously-unhandled import stub, and the fix is now load-
bearing in the committed script, verified live at the exact address
r106 pinpointed. Choosing 384MiB "available" over some other plausible
figure is a documented default-stub value in the same spirit as this
file's pre-existing `KeQueryPerformanceFrequency = 50000000u` -- a
deliberate, justified HLE choice, not an assertion about real hardware's
exact runtime state, which this project has no way to observe.

The probe's new stopping point (still parked at the 30s bound, no crash,
no further console output) is real progress -- GATE2's chain is closed
-- but its own cause is unestablished this cycle. This is squarely the
frontier the standing plan (`~/.claude/plans/vectorized-swimming-sunset.md`)
already named: a post-IB wait/scheduler stall the probe's minimal thread
bring-up does not yet resolve. That plan's own Step 1 (static
verification of what `sub_821E6AC8`/`sub_821E61A8`/`sub_821E64A8` -- or
whatever the guest now actually reaches, since the call graph past
GATE2 has not been re-traced since r100's crash made it moot -- actually
wait on) is the correct next step, not a fresh guess.

## Gates

- `python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`
  -- fails on `playable_session evidence size mismatch:
  reconstruction/ace-combat-6/src/retail_session.cpp`, the same
  pre-existing, unrelated failure r107 and prior cycles reported (a
  different product tree, `reconstruction/ace-combat-6`, untouched this
  cycle).
- `ctest` (native profile): 9/9.
- `git status` (run after `ctest`, not before): no regenerated metrics
  artefact came back modified-but-unstaged.
- `git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`:
  185, unchanged.
- Python suite: `tests/test_materialize_native_import_stubs.py` 18/18.

## Next

1. Re-run the standing plan's Step 1 against whatever the guest now
   actually reaches past the cleared GATE2 (do not assume it is still
   `sub_821E6AC8` -- that call graph has not been re-traced since r100's
   crash superseded it). Identify a real kernel-wait import vs. a
   direct-memory-poll loop before writing any signal-path implementation,
   per that plan's own instruction.
2. r100's `sub_821E6AC8`/`sub_821F03B0` wait-chain thread, carried as
   "probably moot" across r101-r107, needs re-evaluation now that
   execution plausibly reaches further than any of those cycles' traces
   assumed -- it may no longer be moot.
