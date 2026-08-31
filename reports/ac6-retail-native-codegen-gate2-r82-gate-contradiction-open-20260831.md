# AC6 retail NTSC-U/J — r81's premature-gate hypothesis is unsupported; a new contradiction is open, unresolved (r82)

Date: 2026-08-31.

## r81's hypothesis checked, and refuted

r81 suggested `sub_82331CA8` (the call `sub_821D5F48` makes at `0x821d6008`,
before it initializes the heap handle at `0x8293B970`) might be "normally
gated by state our HLE stub layer satisfies prematurely." This cycle read
`sub_82331CA8` in full (`0x82331ca8..0x82331d13`, 27 instructions) and the
surrounding stretch of `sub_821D5F48` from function start through
`0x821d6020`: **there is no conditional branch anywhere in either span**.
Both are unconditional, straight-line initialization sequences. This
hypothesis does not hold; nothing found here gates the call.

## A different, unresolved contradiction

Re-reading `sub_821E65B0`'s own gate (already dumped in r79, re-verified
fresh this cycle byte-for-byte against the cached disassembly, no
transcription error):

```
0x821e65c4: lwz r11,0x2a9c(r31); cmplwi cr6,r11,0; beq cr6,0x821e65e0
0x821e65d0: lwz r11,0x30(r31);   cmplwi cr6,r11,0; beq cr6,0x821e65e0
0x821e65dc: bl 0x821e64a8
```

Both `object+0x2a9c` and `object+0x30` must be **nonzero** to reach the call
into `sub_821E64A8` at all — the exact call frame `sub_821E65B0 →
sub_821E64A8` observed on Thread 1's own stack at every stall capture
(r78/r80/r81, reproduced 5+ times now). But r80's live read, at the deeper
stall point inside `sub_821E6AC8`, found `object+0x2a9c == 0` and
`object+0x30 == 0` — on the **same thread, same call stack, no intervening
call found in `sub_821E64A8`/`sub_821E61A8`/`sub_821E6AC8`'s own bodies that
zeroes either field**, and no other of the ~16-18 host worker threads
touches these functions at all (checked: grepped every thread's backtrace
in the r80 session's `thread apply all bt` capture for
`sub_821E6[14568]`-prefixed frames — only Thread 1 ever appears there).

This is a genuine, unresolved discrepancy between a static read (the gate
requires nonzero) and a live read (the fields are zero moments later on the
same call path), and it has not been explained away by re-checking the
static disassembly or by ruling out a concurrent writer.

## Two experiments to resolve it, both inconclusive

1. **Conditional breakpoint** (`break __imp__sub_821E64A8 if
   *(unsigned long*)$rdi == 0x1a0010`, catching the object argument at raw
   function entry via `ctx.r3`, before any register reallocation): never
   fired within a 20s window. `sub_821E64A8` is called from many sites for
   many different objects during boot (r79: 8 static call sites); GDB's
   per-hit software condition evaluation over ptrace is expensive enough
   that it likely never let real execution reach the specific call for
   `0x1a0010` within the probe's usual ~13s stall window. Not a negative
   result — an inconclusive one; the instrument was too slow to use here,
   not evidence the condition never becomes true.
2. **Watchpoints** on `object+0x30`, `object+0x2a9c`, `object+0x2a90`
   (computed host addresses, set after confirming `$r14`): triggered
   repeatedly with `Old value = <unreadable>` / `New value = 0` (and the
   reverse), on **unrelated threads** (`NativeGuestVdService::poll_loop`,
   `NtClearEvent`, `NtWaitForSingleObjectEx` — none of which touch this
   object's functions per the check above). This is consistent with GDB's
   multi-threaded software-watchpoint emulation being unreliable here
   (single-stepping across ~16-18 concurrently running host threads to
   evaluate a software watchpoint is a known source of spurious/misattributed
   stops), not with a real cross-thread write. Discarded as an unreliable
   instrument for this specific setup, per `CLAUDE.md`'s "measure the
   instrument before trusting it."

## Decision

No implementation. This cycle's honest yield is negative: r81's specific
hypothesis is refuted, and the deeper contradiction it was chasing remains
open, with two GDB-based instruments tried and found unfit for resolving it
in this multi-threaded native runtime. The next cycle should not repeat
either experiment as-is. Better candidates, not yet tried:
- reduce contention by running the probe with only Thread 1 alive (e.g. a
  one-off build with `ExCreateThread`'s worker spawn stubbed to a no-op,
  bounded and reverted after the experiment) so a conditional breakpoint or
  watchpoint has a much smaller, single-threaded window to evaluate against;
- or re-derive the answer statically instead: check whether
  `object+0x30`/`object+0x2a9c` could plausibly hold a small nonzero value
  from an *earlier, unrelated* use of the same freshly-allocated memory
  (stack/heap reuse) that happens to satisfy the gate by coincidence rather
  than by design — i.e. question whether the gate at `0x821e65c4` is even
  operating on meaningfully-initialized data the first time this object is
  used, independent of the `0x8293B970` heap-handle question r80/r81 raised.

No native code changed. Two throwaway dump scripts
(`DumpUsSubsystemEntry.java`, `DumpUsGateCheck.java`) were removed after
use.
