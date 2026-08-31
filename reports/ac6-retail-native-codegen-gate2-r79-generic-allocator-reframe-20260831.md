# AC6 retail NTSC-U/J — the wait chain sits on a generic command-buffer allocator, not a GPU-specific ring (r79)

Date: 2026-08-31.

## What this cycle adds to r78

r78 traced `sub_821E61A8`'s blocking condition to `*(object+0x2a90)` (a
pointer, dereferenced) never reaching `object+0x2a9c`. This cycle found the
writer and what gates it.

- **The cursor writer**: `sub_821E5D60` (`0x821e5d60..0x821e5e47`, single
  caller `sub_821E5E48:0x821e5f50`) builds an 8-dword command packet and,
  **only when `object+0x540c == 0` AND a flag bit at `object+0x2abd`
  (mask `0x1e,0x1e`) is set**, writes `object+0x2a9c` (the current limit
  value) into `*(object+0x2a90)` (`0x821e5e2c`/`0x821e5e30`) — this is what
  would satisfy `sub_821E61A8`'s wait. It unconditionally advances
  `object+0x2a9c` by 2 regardless (`0x821e5e40`).
- **The call chain that reaches it**: `sub_821E60A8` (a generic "flush"
  entry point) → `sub_821E5E48` (`0x821e6138`, right after the
  `bl 0x821e54b8` gate at `0x821e6110` returns nonzero) → `sub_821E5D60`.
- **The gate, `sub_821E54B8` (`0x821e54b8..0x821e564f`)**, is not a boolean
  check — it is a **suballocator**: it early-returns 0 if
  `object+0x2abd` bit `0x04` (mask `0x1a,0x1a`) is set; otherwise it either
  bump-allocates from a pool object at `object+0x34bc` (fields `+0x98/+0x9c`,
  or an indirect device call through `+0xac(pool)` when the pool is empty,
  `bctrl` at `0x821e55d0`) or falls back to `sub_821E52B0`. **On allocation
  failure it sets `object+0x2abd` bit `0x20`** (`0x821e563c..0x821e5644`) —
  a self-reported "out of space" flag.

## Reframe

`sub_821E60A8` (the entry point that reaches this chain) has **76 static
call sites** spanning almost the entire image (`0x820fd2e8` through
`0x821f1780` — outside the `0x821Exxxx` cluster entirely at the low end).
This, plus `sub_821E54B8` being a general-purpose pool/bump allocator with
its own budget tracking (`object+0x3a44` vs `object+0x3a48`) and fallback
device call, means the structure this whole chain operates on
(`object`, whatever it is at the live stall) is a **generic linear
command/data buffer allocator abstraction reused across the engine** — not
demonstrably the Vd/PM4 graphics ring specifically. r77's read of the pushed
packet (`0x5c8`, `0x00020000`) already didn't fit a PM4 type-3 header; this
is the structural confirmation why: it's a different, more generic
mechanism.

## Decision: stop static call-graph tracing, recommend runtime inspection

Enumerating callers of a 76-site, generically-reused allocator entry point
is not a productive way to identify which concrete instance Thread 1 is
blocked on. The available static evidence has diminishing returns here.

The next actionable step is a **runtime read**, not another static scan:
re-run the existing bounded, opt-in entry probe
(`AC6_NATIVE_ALLOW_ENTRY_PROBE=1`, same bounds as r51/r75/r76) under GDB,
break at the live PC inside `sub_821E6AC8`/`sub_821E61A8` (already known from
`entry-gdb-all/gdb.log`), and read out, for the actual live `object`
pointer (`r31` in `sub_821E61A8`'s frame):
- `object+0x2abd` (the flag byte — is bit `0x04` set, blocking the
  allocator outright? is bit `0x20` set, meaning it already failed once?);
- `object+0x540c` (must be exactly 0 for the cursor-advance branch to even
  be reachable);
- `object+0x34bc` (the pool pointer — null or valid?);
- `*(object+0x2a90)` and `object+0x2a9c` (the two compared values,
  concretely, not just symbolically).

That single read will show which of the three explanations holds: (a) the
allocator pool is simply exhausted and nothing ever calls the flush chain
to free it, (b) the flag-bit gate is blocking the advance even though flush
runs, or (c) something else entirely. Guessing among them without the
values would risk exactly the synthetic-fix mistake this thread has
avoided twice already (r53, r77).

No native code changed this cycle. `scripts/FindDerefWritesAtDisplacement.java`
(new, read-only) is the tool that found the `sub_821E5D60` writer; kept.
`DumpUsCursorWriter.java`, `DumpUsFlushGate.java` are throwaway one-target
dumps and are not carried forward as reusable tools (removed).
