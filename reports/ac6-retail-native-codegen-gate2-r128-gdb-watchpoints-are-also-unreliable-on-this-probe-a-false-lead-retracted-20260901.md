# AC6 retail NTSC-U/J — GDB watchpoints are also unreliable on this multi-threaded probe (extending r104); a first-attempt lead retracted (r128)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle** -- static grepping
of already-generated source plus `gdb --batch` sessions against the
already-built binary. `git status` on `recompilation/ace-combat-6-retail/`
confirmed clean before and after (only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change).

## Continuing r127's frontier: who writes `0x8293b93c`?

r127 left finding the writer of the handle-table entry at
`0x8293b93c` as the open question. Two static approaches were tried
first, both exhaustive and both negative:

1. `FindPpcAddressMaterialization.java` (read-only) against
   `0x8293b93c` directly: no `lis`+`addi`/`ori` pair anywhere in the
   XEX builds this exact address.
2. Grepping every generated `PPC_STORE_U32(..., -18116)` site (the
   displacement `sub_821CC508` itself uses to reach this table) across
   the whole codebase: exactly two direct stores use that displacement,
   both against a *different* base constant (`-32099`, not `-32108`)
   -- confirmed by reading their surrounding context -- so neither
   writes our target address.

Also checked whether the value might simply be the XEX's own
compile-time static initializer rather than a runtime write:
`DumpBytes.java` (read-only) at `0x8293b93c` shows the static image is
all zero, not `0xFFFFFFFF` -- ruling that out. Something at runtime
does write `INVALID_HANDLE_VALUE` here, through an addressing pattern
neither static technique above covers.

## A new technique tried: a GDB watchpoint

Since the crash is deterministic (r116) and interactive breakpoint
*stepping* was already retired for this probe (r104), a passive
hardware watchpoint (set once, then `continue` -- no interactive
stepping) was tried as a different GDB feature, not a repeat of the
already-retired technique:

```
break __imp__sub_821F4E70
run --probe-entry <assets>
watch *(unsigned int *)($rsi + 0x8293b93cul)   ; $rsi = base at fn entry, SysV ABI
continue
```

**First attempt**: the watchpoint fired, reporting `Old value =
4294967295` (`0xFFFFFFFF`, consistent with r127) and a backtrace
through `sub_82346428` -- the exact function r111-r116's earlier
background-thread crash investigation centered on. This looked like a
genuine, exciting connection between the two halves of this session's
investigation.

## Retracted: two repeat attempts show the watchpoint is spurious, not real

Per this project's own "measure the instrument" discipline (`CLAUDE.md`
cites four prior versions of one scan before it found anything real),
the same watchpoint was set again, twice, with identical setup. Both
repeats fired -- but with **completely different, mutually
inconsistent backtraces**, neither resembling the first:

```
attempt 1 (this cycle's 2nd run): wait_event() <- NtSignalAndWaitForSingleObjectEx
attempt 2 (this cycle's 3rd run): a raw pthread_cond_wait/futex wait, no guest frames at all
```

`New value = <unreadable>` in every single capture, including the
original. **This is not a real write event being caught three
different times in three different places -- it is GDB's watchpoint
mechanism producing spurious hits in this specific heavily
multi-threaded (eighteen-thread) probe**, most plausibly a debug-
register/thread-context-switch artifact rather than three genuinely
different writers of the same address.

**The `sub_82346428` connection from the first attempt is explicitly
retracted.** It was a plausible-looking, even satisfying lead
(tying back to r111-r116's own investigation), and it would have been
easy to report as a real finding without the repeat check -- exactly
the kind of plausible-but-uncontrolled claim this project's own
standard (cycles 1111/1113, cited in `CLAUDE.md`) exists to catch.

## Decision

This extends r104's finding (interactive GDB breakpoint *stepping* is
unreliable on this probe) to a second, different GDB feature:
**hardware watchpoints are also unreliable here**, for what is very
plausibly the same underlying cause -- eighteen real concurrent
threads (r111-r116) straining GDB's per-thread debug-register handling
in a way this specific probe seems to trigger more than typical
multi-threaded programs. Future cycles should not reach for GDB
watchpoints on this probe either, without independent confirmation via
the build-tree instrumentation technique this project has used
reliably since r105.

The writer of `0x8293b93c` remains unidentified. Both static techniques
tried are exhausted for this specific address; the live technique
tried is now known unreliable. No native code changed.

## Gates

No native code changed. `git status` on
`recompilation/ace-combat-6-retail/`: clean, unchanged from before this
cycle.

## Next

1. Do not use GDB watchpoints on this probe without independent
   confirmation (matching r104's existing guidance for breakpoints).
2. Find the writer via this project's own validated reliable
   technique instead: build-tree `fprintf` instrumentation
   (temporary, reverted before commit, as used since r105) placed at
   plausible candidate sites -- starting with `sub_82346428` itself
   (still a live, if unconfirmed, candidate given the first watchpoint
   attempt's coincidence, now to be checked properly rather than
   assumed) and the `Function_82390F48`/hard-drive-partition cluster
   r122/r123 traced, since neither has been ruled out by a reliable
   method yet.
3. Alternatively, a broader static sweep for the address-materialization
   *pattern* this table base uses (`lis` to `-32108`, whatever
   instruction sequence follows it in the actual writer, which is
   apparently not a simple `addi`/`ori` immediately after) may still
   turn up the site with a more targeted script than the two already
   tried.
