# AC6 retail NTSC-U/J — the real path is gated by a global mode byte = 2, and the `-1` comes from a bounded retry loop giving up on a per-thread status check (r119)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** One round of
`AC6_R119_DIAG`-gated instrumentation was added to and fully reverted
from the generated (gitignored, untracked) `ppc_recomp.22.cpp` --
confirmed via `grep -c "r119"` returning 0 and `ctest` 9/9 passing
afterward.

## Continuing r118's frontier, and correcting an assumption made moments into this same cycle

r118 named the global mode byte at `lis r21,-32108` / displacement
`-18120` as the real gate deciding which of `sub_821CC508`'s three
internal dispatch regions actually runs, and named reading its live
value as the next step. Before instrumenting it, this cycle's own
static reading assumed the byte was `0` (based on it "not equaling 1"
and reasoning about which branches were consistent with r118's failed
instrumentation) and spent real effort tracing a `loc_821CCD4C` region
on that assumption -- a region whose only terminal return turned out to
be `+1` (success), already a sign something was off.

**Direct instrumentation of the byte's actual value corrected this
immediately:**

```
[r119] global mode byte at r21-18120 = 2 (addr=0x8293b938)
```

The byte is `2`, not `0` or `1`. This is named explicitly as a second,
avoidable guess this cycle nearly repeated the same mistake r118 just
corrected -- static reasoning about which branch is "consistent" is not
a substitute for reading the value.

## The real path: `loc_821CC800`'s own fallthrough (byte == 2)

With the byte confirmed `2`, `sub_821CC508`'s control flow is:
`loc_821CC534` (byte != 1, branch to `loc_821CC800`) -> `loc_821CC800`
(byte == 2, **falls through**, does not branch to `loc_821CCD4C`) -- a
third, genuinely distinct case-dispatch table
(`generated/ppc_recomp.22.cpp` line 35884 onward, jump table base
`lis r12,-32227 / -14244`, cases 0-10 with their own targets, entirely
separate from both the byte==1 switch r109/r117 examined and the
byte==0-consistent `loc_821CCD4C` region this cycle initially chased).

## Tracing this switch to its actual `-1` origin: a bounded retry loop

Searching this switch's whole body (`generated/ppc_recomp.22.cpp` lines
35884-37066) for literal `-1` assignments found exactly one, at
`loc_821CCCD8` (line 36577), reached from a retry-counter check at
`loc_821CCAB0` (line 36269):

```cpp
loc_821CCAB0:
	r11 = PPC_LOAD_U32(r31 + 22896);        // retry counter
	r10 = r11 - 1;
	PPC_STORE_U32(r31 + 22896, r10);
	if (r11 == 0) goto loc_821CCCD8;         // counter already exhausted -> give up
	PPC_STORE_U32(r31 + 324, r15 /* = 3 */); // else: set state=3, retry
	goto loc_821CC838;                       // loop back

loc_821CCCD8:
	PPC_STORE_U32(r31 + 320, <bit0 cleared>);
	sub_821D4988(ctx, base);   // args shaped like an error/diagnostic log call
	ctx.r3.s64 = -1;
	return;
```

**This is a bounded retry loop that gives up and logs an error after
its counter (guest offset `+22896` of the state object) reaches zero.**
The check gating whether a retry happens or the loop succeeds
(`loc_821CCAB0` is only reached on failure) is, tracing one level back
(line 36241-36258), **the exact same helper pair r117 already read**
while investigating (at the time, wrongly-attributed) `case 3`:
`sub_821F4E70` then `sub_821F50A0` (which tail-calls `sub_821F75F0`,
already read in r117 as reading `ctx.r13.u32 + 336`, a per-thread
value, returning `0` if that field is zero or the value at
`[[ctx.r13+256]+352]` otherwise). Success (skip the retry-count
decrement) requires `r30 == 1`, or the returned status `== 0`, or
`== 997`; the retry/give-up path is taken on any other value.

r117's earlier reading of this exact helper pair was not wasted -- it
correctly identified the status-check *mechanism*, just under the
wrong switch/case attribution at the time.

## Decision

This is real progress that corrects two chained assumptions within a
short span (this cycle's own initial byte guess, layered on r117's
state-value guess) rather than letting either stand. The `-1` this
whole investigation arc has been chasing since r109 now has a fully
concrete, traced origin: a retry loop bounded by a counter at guest
offset `+22896`, gated on a per-thread status field at
`ctx.r13.u32 + 336` (or, if that is set, an indirected value two levels
further through `ctx.r13`) never settling to `0`/`1`/`997` before the
retries run out.

Neither the retry counter's initial value nor what `ctx.r13+336`
actually holds in this harness was read this cycle. Guessing either is
refused, per this project's standard.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`recompilation/ace-combat-6-retail/`: clean (instrumentation fully
reverted before this report, confirmed via `grep -c "r119"` returning
0).

## Next

1. Read/instrument `ctx.r13` itself in this harness -- what does this
   probe's per-thread register actually point to, and does anything
   ever write a real value to offset `+336` of whatever it points to
   (or to `[[+256]+352]` if `+336` is nonzero)? This is the concrete
   field the retry loop is waiting on.
2. Separately, read/instrument the retry counter's initial value at
   guest offset `+22896` of the state object -- if it starts small, a
   genuinely slow (but eventually-successful on real hardware) I/O
   completion could look identical to a permanent failure in this
   harness, which is a different class of gap than an always-wrong
   status value.
3. Given the shape (997-adjacent status checks, retry-then-fail,
   `sub_821D4988` as an apparent diagnostic/log call before giving up),
   confirming what `sub_821D4988` actually logs (a static string
   argument is visible at the call site) could name the failing
   subsystem directly without further register tracing -- read that
   string before instrumenting further.
