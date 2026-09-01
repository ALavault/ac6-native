# AC6 retail NTSC-U/J — `sub_821D4988` posts an async message to a ring buffer, not a printf-style log call; r119's speculation corrected (r120)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle, used
read-only for `DumpBytes.java`), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle** -- pure static
reading (generated source plus one read-only Ghidra byte dump). `git
status` on `recompilation/ace-combat-6-retail/` confirmed clean before
and after (only the pre-existing, unrelated `upstream/AC6_recomp`
submodule pointer change).

## Continuing r119's frontier, and correcting one of its own speculations

r119 named reading `sub_821D4988`'s call-site string (visible as a
static argument, `ctx.r3`) as a cheap way to potentially name the
failing subsystem directly, describing the call as having "args shaped
like an error/diagnostic log call."

Computed the guest address from the two-instruction materialization at
the call site (`generated/ppc_recomp.22.cpp` lines 36561-36566:
`lis r11,-32138; addi r3,r11,-23532` -> `0x8275a414`) and read it with
`DumpBytes.java` (read-only): **128 bytes of zero**, not a string.
r119's "diagnostic log" framing was speculation from the call shape
alone, not verified -- corrected here rather than left standing.

## What `sub_821D4988` actually does

Read the function itself (`generated/ppc_recomp.23.cpp` line 15550
onward). It is not a logger:

```cpp
// r3 = constant (0x8275a414, all-zero guest memory -- likely a
//      message-type tag/opaque ID, not a string), r4 = caller's local
//      buffer pointer, r5 = 0
RtlEnterCriticalSection(...);        // real mutex since r116
// packs {r4, r5} into an 8-byte slot in a circular buffer at
// r31+524, indexed by a write cursor at r31+516 (mod 64), count at
// r31+520 -- a fixed-size ring buffer, classic producer pattern
RtlLeaveCriticalSection(...);
sub_821F5988(4, r31+568, 0, 0);      // then waits/signals something
if (result == 258 /* STATUS_TIMEOUT-shaped */) ...
```

**This posts a message onto a ring buffer, then calls a wait/signal
helper (`sub_821F5988`) checking for a `258`-shaped result** -- the
same `0x102`/`STATUS_TIMEOUT` family of values this codebase's own
`wait_event()`-backed stubs already use elsewhere. This is a real,
protected (post-r116) producer writing into a bounded queue and then
waiting on it -- not a `printf`-style diagnostic. `r3`'s zero-filled
target is consistent with it being an opaque numeric tag (a message
type or component ID) rather than a string pointer at all.

## Decision

This does not change r119's established causal chain (the retry
counter at `+22896` is what actually decides whether the loop retries
or gives up; `sub_821D4988` is a side effect of giving up, not a
determinant of it) -- but it corrects an unverified description within
that report before it could mislead a future cycle into looking for a
readable error string that does not exist. `sub_821D4988` is better
understood as an asynchronous notification/telemetry post than a log
call; what consumes this ring buffer (presumably one of the eighteen
`ExCreateThread`-spawned threads) is unexamined and not necessarily
relevant to why the retry loop's own status check fails in the first
place.

## Gates

No native code changed. `git status` on
`recompilation/ace-combat-6-retail/`: clean, unchanged from before this
cycle.

## Next

r119's own next steps remain the live frontier, unaffected by this
correction:

1. Instrument `ctx.r13` in this harness -- what it points to, and
   whether anything ever writes offset `+336` (or the further-indirected
   `[[+256]+352]`) of whatever it points to. This is the actual field
   the retry loop's status check depends on.
2. Instrument the retry counter's initial value at guest offset
   `+22896` of the state object, to rule in or out a "too few retries
   for a slow-but-real completion" explanation distinct from "the
   status never becomes ready at all."
