# AC6 retail NTSC-U/J — the thread-stack allocator confirms the DATA.TBL garbage is same-thread call-history-dependent; no further bounded native fix exists without a general-fidelity investment (r166)

Date: 2026-09-01.

## Qualification

Static reading of
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`'s
own `ExCreateThread` implementation (hand-maintained, real, committed
source — not generated code). No native code changed this cycle; no
build touched.

## What was checked

r161's own "Next" section named the remaining open question precisely:
"what does this project's own recompilation's stack allocation strategy
do differently that makes catastrophic garbage the common case here."
This cycle answers the mechanism half of that question.

`ExCreateThread`'s stub (line 342 onward) allocates each new guest
thread's stack from a single, process-wide, monotonically-decreasing
counter:

```cpp
std::atomic<std::uint32_t> g_next_thread_stack{0x8ef00000u};
...
worker.r1.u32 = g_next_thread_stack.fetch_sub(0x10000u);
```

Every spawned guest thread gets its own fresh, non-overlapping 64 KiB
slice, counting down from `0x8ef00000`, **never reused across threads**.
The underlying guest address space is a single `mmap(MAP_ANONYMOUS)`
region (`native_guest_memory.cpp`, r150-adjacent evidence already
established this session) — Linux guarantees anonymous pages are
zero-filled on first touch. Combined, this means: the *very first* byte
any thread ever writes at any given offset within its own 64 KiB slice
reads as zero; any *later* read at that same offset reflects whatever
that same thread's own earlier execution wrote there.

## What this confirms

The uninitialized `[r1+88]` read this whole arc has traced (r130-r165) is
**same-thread, call-history-dependent** — not cross-thread contamination,
and not a failure of the allocator to zero-initialize fresh memory (the
allocator and the OS both already guarantee zero for genuinely
first-touch bytes). The garbage r151 measured (`0xfeffffee`), and its
different value after r145/r148/r149's fixes, comes entirely from **this
same thread's own earlier function calls**, at the same or a deeper stack
depth, writing something at this exact relative offset before control
ever reaches `sub_82338568` — consistent with, and now fully explaining,
every earlier finding in this arc (r150's "whatever the stack happened to
hold", r159's "each environment has its own execution history", r161's
measured contrast between this project's `0xfeffffee` and Xenia Edge's
`14824`).

## Why no further bounded fix exists

Explicitly zero-filling each fresh 64 KiB slice at `ExCreateThread` time
would change nothing: the corruption is not a stale-page problem (the
page already reads zero on first touch), it is a **same-thread stack
depth collision** that happens strictly after thread creation, as the
thread's own call graph runs. The only way to change what ends up at this
exact offset is to change what this project's own guest thread actually
does before reaching `sub_82338568` on this run — which is exactly what
r145/r148/r149 already did, each for its own independent, real-bug reason,
each incidentally changing this value without ever making it safe. There
is no single, bounded, principled lever left: making this value safe on
purpose would mean either (a) a broad, open-ended general-fidelity
investment (implementing more of this project's still-generic HLE
surface accurately enough that the *natural* resulting call sequence
happens to leave a benign value here, mirroring how Xenia Edge's own,
much more complete emulation apparently does), or (b) hardcoding a
specific value to match what one particular working environment
happened to produce — exactly the synthetic-value pattern this project's
own discipline refuses (r53's precedent, reaffirmed r159/r160).

## Decision

This closes the mechanism-level half of the question r161 left open,
completing the DATA.TBL investigation arc at both levels: r161 already
established *that* this project's recompilation is the catastrophic
outlier and real/emulated execution is not; this cycle establishes
*why*, mechanically, that is expected rather than a fixable specific
defect. No further work is queued on this specific thread. Any future
progress here rides on this project's general HLE completeness improving
over time (as r145/r148/r149/r162/r163/r165 have each done, on their own
merits), not on a targeted patch for this one symptom.

## Gates

No native code changed, no build touched. `ctest`'s last-known state
(9/9, r165) stands unaffected. `git status` unchanged (only pre-existing,
unrelated dirty state).

## Next

Both of Gate 2's named frontiers remain in their settled states: DATA.TBL
chain fully traced, understood at both the value level (r161) and the
mechanism level (r166), with no further bounded native lever;
`IM_LOAD_IMMEDIATE`→SPIR-V remains policy-blocked pending Xenos
fetch-signature qualification. The contract-shape sweep (r148-r165) is
complete. Remaining named options, per r164/r165: a genuinely new
investment (the pinned Wine/Xenia route for NTSC-U/J, not yet attempted),
continued incidental fidelity improvement to any other still-generic HLE
surface as it is found, or a different frontier entirely.
