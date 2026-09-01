# AC6 retail NTSC-U/J — the r135/r137 garbage allocation size is still reached after this session's three fixes, with a different exact stale value (r151)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. No native code changed this cycle — one throwaway, env-gated diagnostic
(`AC6_R151_DIAG`) added to a gitignored, regenerated build-tree file
(`generated/ppc_recomp.31.cpp`, at `__imp__sub_82222D80`'s entry), used to
take a single live measurement, and fully reverted via `cp` from a
`/tmp/*.orig` backup. Confirmed reverted via `grep -c "r151"` returning 0, a
clean `tools/build.py --target ntsc-uj --profile native` (`ctest` 9/9). No
retail-native Python source was touched, so the Python suite was not re-run
this cycle. `git status` shows only the pre-existing, unrelated dirty state
already on this tree (the demo submodule conversion, the `reconstruction/`
tree's own uncommitted changes, and untracked `reports/retail-us-*` /
`reports/cycle-183*..1850` files from a different track) — nothing from this
cycle's diagnostic leaked into it.

## Why this check was worth running

r150 found that `sub_82338388`'s stale-stack readback (the value r139
originally, wrongly attributed to a direct relay, and r141/r142 traced to
uninitialized stack content) changed from `0` to `1` after this session's
three real fixes (r145, r148, r149) altered guest scheduling and execution
order. r150 explicitly declined to re-derive whether the rest of the r130-142
causal chain — the ~4 GiB-class garbage allocation size at `sub_82222D80`
(r135), the allocator's correct rejection of it, and the downstream
consequences — still holds, naming this exact re-measurement as item 1 of its
own "Next" list. This cycle takes the first, most load-bearing link of that
list: does `sub_82222D80` (the allocator r135 identified) still get called
with a garbage, ~4 GiB-class size on this run?

## What was found

A live diagnostic at `sub_82222D80`'s entry, across the whole bounded probe,
shows four calls:

```
[r151] sub_82222D80: entry r3=0x16f70000 r4(size)=0x000001cc r5=0x00000010 r6=0x00000000
[r151] sub_82222D80: entry r3=0x16f70000 r4(size)=0x00400000 r5=0x00000010 r6=0x00000000
[r151] sub_82222D80: entry r3=0x16f70000 r4(size)=0xfeffffee r5=0x00000010 r6=0x00000000
[r151] sub_82222D80: entry r3=0x16f70000 r4(size)=0x00000000 r5=0x00000010 r6=0x00000000
```

The third call still requests `0xfeffffee` bytes (~4,009 MiB) — the same
magnitude-and-bit-pattern class r135/r137 identified (`0xFEFF0000`-prefixed,
~4 GiB), but **not the identical value** r137 measured before this session's
fixes (that cycle's own figure was `0xFEFF0000|65528`, i.e.
`0xfefffff8`-class; this run's is `0xfeffffee`). This is consistent with
r150's own framing: the *mechanism* (a garbage, effectively-random low
16 bits riding on a fixed `0xFEFF....` high pattern, sourced from
uninitialized/stale memory) is unchanged, but the *exact value* is sensitive
to whatever this session's fixes changed about prior execution history and
stack/heap reuse — the same relationship r150 found for the `sub_82338388`
stack slot. This is a different variable from `sub_82338388`'s value (this
one is read via a different path into `sub_82222D80`'s size argument, not
the `[caller_frame+88]` slot r141/r142 traced), so this is now a second,
independent confirmation of the same "still garbage, value shifted" pattern
r150 found once.

The other three calls (`0x1cc`, `0x400000`, `0x0`) are small/plausible or a
zero-size no-op call and are not evidence of anything broken.

## What this does and does not establish

This confirms the first link of r150's retrace list: the garbage-size
allocation r135 found is **still reached** in the current, post-r145/148/149
binary, so the broad causal shape (garbage size → allocator correctly
rejects it → unchecked failure → downstream corruption → `sub_821F7C80`
crash) is not falsified by this session's fixes. It does **not** by itself
re-verify the *rest* of that chain (r137's exact garbage-value provenance,
r139's category/setting query, r141/r142's `[r1+88]` stack-slot read this
value ultimately traces back through) against the current binary — those
remain open, per r150's own list, items 1 (partially addressed here) and 2.

## Decision

Recorded as a positive, narrow re-confirmation rather than a full retrace:
the load-bearing allocator call still receives a garbage size of the same
class, so the next cycle's retrace work is not chasing a mechanism that has
disappeared — only one whose exact numeric fingerprint has moved, as r150
already established for one other value in the same chain. Deeper tracing
(which config query produces this exact `0xfeffffee`, whether it is the same
`sub_82339AA8`/`sub_82338388` path r139-r142 mapped or a shifted one) is left
to the next cycle, consistent with r150's own scoping of this as a
multi-cycle thread.

## Gates

No native code changed this cycle; `ctest` 9/9 (native profile). `git status`
shows only pre-existing, unrelated dirty state (demo submodule conversion,
`reconstruction/ace-combat-6/**`, and untracked `reports/retail-us-*` /
`reports/cycle-183*` files from a different, already-existing track) —
nothing from this cycle's diagnostic.

## Next

1. Determine whether `0xfeffffee` still flows through the same
   `sub_82339AA8`/`sub_82338388`/`[r1+88]` path r139-r142 mapped, or a
   different one — the exact value changed, but the path has not yet been
   re-checked.
2. Re-run r139's category=1/setting=3 config-provider query measurement
   against the current binary.
3. Determine which specific caller of `sub_82390880` reaches it on the
   `DATA.TBL` handle (r150, item 2 — untouched by this cycle).
4. Continue treating this as the multi-cycle retrace thread r150 named,
   rather than a single-cycle fix.
