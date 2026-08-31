# AC6 retail NTSC-U/J — the timeout escalation is pure telemetry, and the object address is unexplained (r83)

Date: 2026-08-31.

## Checked and ruled out

`sub_821E6A08` (`0x821e6a08..0x821e6ac7`), the function `sub_821E61A8` calls
when its bounded backoff via `sub_821E6AC8` times out — never previously
examined — was read in full. It does **not** touch `object+0x30`,
`+0x2a9c`, `+0x2a90`, `+0x2af8`, or `+0x2abd`. It operates on a *different*
pointer entirely (`r3` here is the local stack scratch buffer
`sub_821E61A8` built at `r1+0x50`, whose first word is a separate
"owner/profiler" object, not the ring object) and is purely a performance
counter: accumulates elapsed time (`mftb`-derived) into one of two global
running totals depending on a threshold, then conditionally invokes a
registered profiling callback (`bctrl` through `owner+0x346c`) with
float-computed elapsed-time arguments. It cannot explain r82's contradiction
— it is telemetry, not cleanup.

## A new, unexplained fact

`object = 0x1a0010` (the live-read guest address, reproduced 5+ times
across r78/r80/r81/r82/this cycle) is checked against the actual Ghidra
memory map: **entirely outside the static XEX image** (`0x82000400`..
`0x82ac2dc7`, all sections listed, none containing it) **and below the
`NtAllocateVirtualMemory` bump-allocator's own range**
(`0x10000000..0x7f000000`, per r47/the Explore agent's finding on
`materialize_native_import_stubs.py`). `0x1a0010` (~1.7 MB) is neither
static image content nor a native-runtime kernel-level allocation. Grepped
the native runtime's own sources for any hardcoded reference near this
address — none found.

This does not yet explain the r82 contradiction, but it rules out one more
easy explanation (that `object` might coincide with a native-runtime
bootstrap region like the PCR/thread addresses `0x0f000000`/`0x0f001000`
from r49 — it does not) and narrows what `object` could be: most likely a
pointer returned by the *game's own* heap (the `sub_82222d80` chain r80/r81
traced), which is free to place allocations anywhere below the image,
including this low.

## Decision: stop iterating on this contradiction with static/GDB reads alone

Three cycles (r81, r82, this one) have each proposed and checked a specific
explanation for the r82 contradiction (premature gate — refuted; escalation
path resets fields — refuted) without resolving it, and two GDB instruments
(conditional breakpoint, watchpoints) were already found unfit for this
multi-threaded target (r82). Continuing to guess at explanations one at a
time is exactly the diminishing-returns pattern r79 already flagged for
static call-graph tracing.

The concrete next action, not yet attempted: build the bounded, single-
thread probe variant r82 proposed — a temporary, uncommitted local change
that no-ops `ExCreateThread`'s worker spawn (or gates it behind an env var
already read at startup) so the probe runs with only the guest's main
thread live, eliminating the ~16-18 concurrent host threads that made the
watchpoint experiment unreliable. Rebuild, rerun the same GDB technique
(break at `sub_821E65B0` entry, single-step to the gate check at
`0x821e65c4`, read `object+0x2a9c`/`+0x30` at that exact point before
continuing) to get one clean, uncontended answer. Revert the temporary
change afterward; it is an experiment, not a fix, and does not belong in
the committed native runtime.

No native code changed this cycle. One throwaway dump script
(`DumpUsEscalation.java`) and one throwaway check script
(`CheckUsAddressMapping.java`) were removed after use.
