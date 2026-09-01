# AC6 retail NTSC-U/J — heap creation succeeds with a real handle (`0x16F70000`); the failing 16-byte allocation is inside the allocator's own empty-free-list/grow logic, not a missing heap (r136)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** One round
of throwaway, env-gated diagnostic instrumentation (`AC6_R136_DIAG`) was
added to a gitignored, regenerated build-tree file
(`generated/ppc_recomp.23.cpp`), used to take a live measurement, and
fully reverted via `cp` from a `/tmp/*.orig` backup. Confirmed reverted
via `grep -c "r136"` returning 0, a clean `tools/build.py --target
ntsc-uj --profile native` (`ctest` 9/9), and 139/139 Python tests. `git
status` on `recompilation/ace-combat-6-retail` shows only the
pre-existing, unrelated `upstream/AC6_recomp` submodule pointer change.

## Starting point: r135's open question

r135 closed the causal chain down to `sub_82222D80` (a real, compiled
guest heap allocator) returning NULL for a 16-byte allocation, and left
open whether this project's native runtime is missing a heap-setup step
real hardware performs earlier, or whether this is real, correct game
behavior reaching a genuinely empty/exhausted heap for the first time.

## The heap-creation gate, traced and measured

Immediately before the call chain r135 identified, `sub_821D5F48` (the
same retry-loop function this investigation has traced since r117) itself
gates heap creation on its own first argument:

```cpp
r31 = ctx.r3;                    // sub_821D5F48's own arg1
if (r31 == 0) goto loc_821D6138; // skip heap creation entirely
...
ctx.r3 = r31;
sub_82221DD0(ctx, base);         // heap init, in place, at *r31
[global heap handle] = ctx.r3;   // store the returned handle
```

A live diagnostic at this exact gate, run against the qualified ISO,
shows the gate is **not** the problem:

```
[r136] sub_821D5F48 heap-create gate: r31(own arg1)=0x16f70000 (0 skips heap creation)
[r136] sub_821D5F48 sub_82221DD0(pool=0x16f70000) returned heap=0x16f70000, stored globally
```

`r31` is a real, nonzero address (`0x16F70000`), the gate is passed, and
`sub_82221DD0` returns that same address as the heap handle -- a
successful, real heap creation. **This rules out "the heap is never
created" as r135's open question's first branch.**

`0x16F70000` sits well within `GuestAddressSpace::kAddressSpaceSize`
(the full reserved 4GiB, `native_guest_memory.h`), so it is not an
out-of-bounds address relative to this project's own guest memory
reservation either -- ruling out a native-runtime mapping gap as the
mechanism, at least for this specific address.

## Reading `sub_82221DD0`: an in-place control-block init with empty free lists and no size parameter

`sub_82221DD0(r3=pool)` takes **exactly one argument** -- there is no
size/length parameter in `r4` or any other register at its two call
sites. Its body zero-initializes what is structurally a set of free-list
head pointers at a dense run of offsets (4, 8, 12, 16, 20, 24, 28, 30,
32, 36, 40, 44, 46, 48, 52, 56, 60, 62... continuing past what this
cycle read in full) inside the control block at `*r31`, and writes a
fixed vtable-shaped pointer at offset 0. A free-list-head pointer at
`r31 + 100` marks where the allocatable arena is expected to begin.

**Every free list starts empty (all zero).** Nothing in this
initialization reserves or commits any backing memory for the arena
itself -- there is no `size` argument to record, and no call here to a
memory-reservation primitive. This is consistent with a classic small-
object allocator design where the *first* allocation of each size class
is expected to fall through to a "grow the heap" / "commit more pages"
path, not with a pre-populated pool.

## Revised understanding: the failure is inside `sub_82222D80`'s own grow/fallback logic

r135 already read `sub_82222D80`'s call into `sub_82221C68` (a size-class
lookup) followed by a branch: small classes fall through one path, larger
ones call `sub_82222908`. Given every free list starts empty, **any**
allocation -- including this cycle's 16-byte one -- must take whichever
of those paths is responsible for growing the heap when its target free
list has nothing on it. That path, not the heap's existence, is where the
actual failure lives. Neither `sub_82222908` nor the empty-free-list
branch of `sub_82222D80` itself has been read yet.

## Decision

No native code changed this cycle -- one round of temporary,
env-gated diagnostics, reverted and verified reverted (`ctest` 9/9,
139/139 Python, clean `git status`). This cycle narrows r135's open
question rather than closing it: heap creation is real and succeeds, so
the failure is specifically in the allocator's own logic for handling an
empty free list on first use -- most likely a call to grow/commit memory
that itself fails, possibly through an HLE surface this project has not
yet implemented. That call site is named as next step rather than guessed
at.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Read `sub_82222D80`'s full body (only its size-class dispatch was read
   in r135) and `sub_82222908` (the branch taken for larger/non-trivial
   size classes) to find the actual grow/commit-memory call an empty
   free list falls through to.
2. If that call reaches an HLE import (a kernel memory primitive this
   project stubs), check whether it is implemented correctly -- this
   would be a genuine, in-scope native-runtime gap.
3. If it instead depends on guest-side state (e.g. a reserved arena size
   baked into the XEX's own data, or a a companion "commit" call this
   project's probe simply has not reached yet in the boot sequence),
   that changes the next step -- trace that dependency rather than
   assuming an HLE gap.
4. Continue preferring the live-diagnostic-plus-revert technique
   (env-gated, build-tree only, always reverted and verified) over
   GDB-based approaches, which remain unreliable on this 18-thread probe
   (r128).
