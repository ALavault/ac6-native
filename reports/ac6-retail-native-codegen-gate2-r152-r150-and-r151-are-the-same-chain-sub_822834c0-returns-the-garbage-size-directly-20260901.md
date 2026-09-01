# AC6 retail NTSC-U/J — r150's stack-slot measurement and r151's allocation-size measurement are the same call chain; `sub_822834C0`'s own return value is the garbage size (r152)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. No native code changed this cycle — one throwaway, env-gated diagnostic
(`AC6_R152_DIAG`) added to a gitignored, regenerated build-tree file
(`generated/ppc_recomp.31.cpp`, at `__imp__sub_82222D80`'s entry), used to
take one live measurement (a `backtrace()`/`backtrace_symbols_fd()` capture
gated on the exact garbage-size condition r151 found, resolved via
`addr2line`), and fully reverted via `cp` from a `/tmp/*.orig` backup.
Confirmed reverted via `grep -c "r152"` returning 0, a clean `tools/build.py
--target ntsc-uj --profile native` (`ctest` 9/9). `git status` shows only the
pre-existing, unrelated dirty state already on this tree (unchanged from
r151's own qualification) — nothing from this cycle's diagnostic leaked into
it.

## Why this check was worth running

r151 confirmed `sub_82222D80` (the allocator r135 identified) still receives
a garbage ~4 GiB-class size on the current binary, but named as open whether
that size still flows through the same `sub_82339AA8`/`sub_82338388`/
`[r1+88]` path r139-r142 mapped, or a different one. A `gdb` conditional
breakpoint on `ctx.r4.u32` was tried first and did not work — this build has
no DWARF debug info for locals/parameters (only the dynamic symbol table),
so gdb cannot resolve a source-level `ctx` field in a break condition; it
silently never stopped. The working substitute, consistent with this
session's own established methodology, is a host `backtrace()` captured
inside the diagnostic itself, resolved via `addr2line`.

## What was found

The `backtrace()` at the moment `sub_82222D80` receives `r4=0xfeffffee`
resolves to:

```
__imp__sub_82222D80
__imp__sub_821CC288
__imp__sub_821D5F48
sub_821D7DE0(PPCContext&, unsigned char*)
_xstart(PPCContext&, unsigned char*)
main
```

`sub_821CC288` is the *direct* caller — matching r135's original
attribution exactly (the same function r135 first suspected before an
intra-cycle self-correction against `sub_821CC508` earlier in this
session's history; this backtrace independently re-confirms `sub_821CC288`
is the real one). Reading `sub_821CC288` itself (`ppc_recomp.22.cpp:35066`)
shows it has exactly one call before `sub_82222D80`:

```cpp
// bl 0x822834c0
sub_822834C0(ctx, base);
// rotlwi r30,r3,0
r30.u64 = __builtin_rotateleft32(ctx.r3.u32, 0);   // r30 = r3, unchanged
...
// mr r4,r30
ctx.r4.u64 = r30.u64;
...
// bl 0x82222d80
sub_82222D80(ctx, base);
```

`r30` is a verbatim copy of `sub_822834C0`'s return value (`rotlwi x,x,0` is
a no-op rotate — codegen's way of expressing a plain register move), and it
becomes `sub_82222D80`'s size argument (`r4`) with nothing in between. **This
means `sub_822834C0`'s own return value *is* the garbage size** — not a
value it merely helps compute.

`sub_822834C0` is exactly the function r150 already instrumented, at exactly
the call it already measured (`sub_822834C0 → sub_82338388`, returning `1`
this session). r150's own report left open whether `sub_82338388`'s stale
value is what r150 measured, or something r150's cycle did not follow
through to a final consumer. This cycle closes that gap directly: **r150's
measurement and r151's measurement are not two separate observations of
possibly-different mechanisms — they are two points on the same call chain**,
confirmed by static backtrace rather than by inference from proximity.

## What this does and does not establish

This confirms the caller-side half of r139-r142's original chain
(`sub_821CC288 → sub_822834C0 → sub_82339AA8`-family) still holds
structurally against the current binary, and ties r150's and r151's
measurements together as the same mechanism. It does **not** yet re-derive
`sub_822834C0`'s own internal logic (its call into `sub_82339AA8`/
`sub_82339D10`, and from there `sub_82338388`'s `[r1+88]` read) step by step
against the current binary — r150 already measured one link of that
internal chain (`sub_82338388` returns `1`), but the arithmetic connecting
"`sub_82338388` returns `1`" to "`sub_822834C0` returns `0xfeffffee`" has
not been walked this cycle. That is the next, narrower step, and it is a
single function's internals (`sub_822834C0`), not a fresh multi-function
retrace.

## Decision

Recorded as the connective finding r150 and r151 were each missing on their
own: this is one chain, not two independent ones, and `sub_821CC288` is
confirmed (not merely re-asserted) as the real caller via a working
diagnostic technique (`backtrace()`+`addr2line`) that worked where a `gdb`
conditional breakpoint on a source-level field did not, given this build's
lack of DWARF locals. That technique note is itself worth keeping for future
cycles needing a caller identity in this codebase.

## Gates

No native code changed this cycle; `ctest` 9/9 (native profile). `git
status` unchanged from r151's own qualification (only pre-existing,
unrelated dirty state).

## Next

1. Walk `sub_822834C0`'s own body (already known to call `sub_82338388`,
   per r150) to find exactly how a stale-stack `1` becomes a returned
   `0xfeffffee` — this is now a single-function trace, not a multi-hop one.
2. Once that arithmetic is understood, decide whether it is worth a native
   fix (e.g., validating the allocation size before use) or remains purely
   diagnostic, consistent with r144's cost-benefit standard for this
   sub-thread.
3. `sub_82390880`/DATA.TBL caller identification (r150 item 2) remains
   untouched and open.
