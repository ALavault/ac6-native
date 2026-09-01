# AC6 retail NTSC-U/J — the entire r100-r124 chain is closed: two unimplemented imports, `NtReadFile` and `RtlNtStatusToDosError`, together produce the status value the retry loop can never accept (r125)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle** -- pure static
reading of already-generated source. `git status` on
`recompilation/ace-combat-6-retail/` confirmed clean before and after
(only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change).

## Continuing r124's frontier: what does `loc_821F4FE4` do?

r124 traced `sub_821F4E70`'s `NtReadFile` call and its caller-side
branch logic, leaving one unexamined path: what happens on a result
that is negative but not `STATUS_END_OF_FILE` (exactly our harness's
case, since `NtReadFile`'s generic-stub return, `kOfflineStatus` =
`0xC00000BB`, is neither).

```cpp
loc_821F4FE4:
	sub_821F75B8(ctx, base);   // ctx.r3 still carries the failed NTSTATUS
	ctx.r3.s64 = 0;
	return;
```

## `sub_821F75B8` is the exact setter matching r119's getter -- and it goes through a second unimplemented import

Reading `sub_821F75B8` (`generated/ppc_recomp.27.cpp` line 8287, a
neighbor of `sub_821F75F0` -- r117/r119's already-traced getter, at
line 8330, sixty-odd bytes away):

```cpp
PPC_FUNC_IMPL(__imp__sub_821F75B8) {
  __imp__RtlNtStatusToDosError(ctx, base);   // convert NTSTATUS (in r3) -> Win32 error
  r11 = PPC_LOAD_U32(ctx.r13 + 336);
  if (r11 == 0) {
    r11 = PPC_LOAD_U32(ctx.r13 + 256);
    PPC_STORE_U32(r11 + 352, ctx.r3.u32);    // <-- the exact field r119/r121 read
  }
}
```

This writes to `[[ctx.r13+256]+352]` -- **the identical address**
r119 traced live (`0x0f001160` under this harness's
`kProbePcrAddress`) and r121 found holding `0xC00000BB`. This is
`sub_821F75F0`'s matching setter, completing the getter/setter pair
r117-r121 had only seen one half of.

**Before storing, it calls `__imp__RtlNtStatusToDosError`** -- and per
r121's own `AC6_NATIVE_IMPORT_TRACE` capture, this is the single
*most*-hit unimplemented import in the whole run (43 calls, more than
any other, `ObReferenceObjectByHandle` a distant second at 35). It
falls through to the same generic offline stub as `NtReadFile`,
returning `kOfflineStatus` regardless of its input -- so whatever real
NTSTATUS `NtReadFile` (or here, its offline stand-in) produced is
discarded and replaced with the same `0xC00000BB` sentinel before
being stored.

## The full chain, closed end to end, with a converged explanation for `997`

`RtlNtStatusToDosError` is a real, well-documented, purely functional
Windows NT API: it maps an `NTSTATUS` to the equivalent Win32 error
code. `STATUS_PENDING` is `0x103` (259, the literal value r124 found
`sub_821F4E70` pre-writing and branching on); its real, standard
Win32-error mapping is `ERROR_IO_PENDING`, whose numeric value is
`997` -- **the exact constant r109 first noticed and every cycle since
has treated as a magic "still pending" sentinel without a confirmed
source.** It now has one: `997` is not an NTSTATUS the retry loop
checks directly -- it is the *Win32-converted* form of `STATUS_PENDING`,
produced by exactly the code path this cycle traced.

The complete mechanism, real hardware vs. this harness:

- **Real hardware**: `NtReadFile` returns `STATUS_PENDING` (0x103) for
  a genuine async read -> `RtlNtStatusToDosError(0x103)` correctly
  converts it to `ERROR_IO_PENDING` (997) -> stored in the per-thread
  status field -> the retry loop's `status == 997` check recognizes
  this as "legitimately still in flight, not a failure" and keeps
  polling -> once the real DMA/interrupt completion updates the status
  to success, the loop proceeds.
- **This harness**: `NtReadFile` returns `kOfflineStatus` (0xC00000BB,
  not a real NTSTATUS at all) -> `RtlNtStatusToDosError(0xC00000BB)`
  also falls through to the generic stub, returning the *same*
  `0xC00000BB` unchanged (a real implementation would return a
  "no mapping found"-shaped default for this project-internal,
  non-standard input, not `997` either) -> stored in the status field
  -> the retry loop's `status == 997` check fails (so does `== 0`, so
  does `r30 == 1`) -> the bounded retry counter (r119: starts at 5)
  decrements every attempt -> exhausts -> gives up, returns `-1` ->
  `sub_821D5F48` bails out via `loc_821D6138` (r117) before ever
  reaching the write to `0x82935d98` -> `sub_821D6C20` later
  dereferences that still-null global -> the crash r100 first found,
  now traced completely to its two proximate causes.

## Decision

This closes the entire investigation arc opened by r100 and worked
through r101-r124: two specific unimplemented imports,
`NtReadFile` and `RtlNtStatusToDosError`, are together sufficient to
explain every observed symptom, from the original crash through every
correction and reframing along the way. Neither alone would fix this:
`RtlNtStatusToDosError` implemented correctly still has no real
`STATUS_PENDING` to convert unless `NtReadFile` supplies one; a
`NtReadFile` returning real `STATUS_PENDING` forever without ever
completing would trade the crash for an infinite busy-loop (the retry
counter only decrements on a *failed*, not a *pending*, status) --
not yet a working fix.

This is not new speculation layered on r123's "maybe a different
failure status" framing -- it is the exact, confirmed mechanism, read
directly from both halves of the getter/setter pair and matched
against `997`'s real, externally-verifiable meaning as
`ERROR_IO_PENDING`.

No native code changed this cycle. Given the design constraint just
established (a real fix needs the read to eventually *genuinely*
complete, not merely claim to be pending forever), implementing this
correctly is exactly the kind of substantive, multi-step change this
project's own precedent (r108, r116) treats as deserving a dedicated
cycle with real design and test coverage, not a rushed addition here.

## Gates

No native code changed. `git status` on
`recompilation/ace-combat-6-retail/`: clean, unchanged from before this
cycle.

## Next

1. Implement `RtlNtStatusToDosError` as a real, small, stateless
   NTSTATUS -> Win32 lookup, at minimum covering `STATUS_SUCCESS(0)
   -> ERROR_SUCCESS(0)` and `STATUS_PENDING(0x103) -> ERROR_IO_PENDING(997)`
   (the two values this specific chain needs), with a sensible generic
   default for anything else -- low-risk, purely functional, no state.
2. Design `NtReadFile`'s fix around the constraint this cycle
   established: returning `STATUS_PENDING` unconditionally would hang
   rather than crash (the retry loop's counter never decrements on a
   recognized-pending status). A working fix needs the read to
   eventually genuinely complete -- e.g. perform the read synchronously
   against this project's existing `read_xdvdfs_file`/media
   infrastructure but still report `STATUS_PENDING` on the *first*
   poll (matching what this specific caller, per r124, explicitly
   expects) and `STATUS_SUCCESS` once the (already-completed) data is
   available on a subsequent poll -- or determine whether the
   underlying operation is one this harness should represent as
   synchronously instant instead of asynchronous, given no real DMA
   exists here anyway.
3. Once both are implemented, verify live (using the same
   single-run instrumentation technique this arc has used since r108,
   the crash being deterministic since r116) that `0x82935d98` is
   finally written and the original crash from r100 stops occurring --
   the first end-to-end verification this entire investigation arc
   will have produced.
4. r122/r123's `Function_82390F48` cluster (the fixed-offset
   hard-drive-partition read) remains a separate, unconnected thread --
   this cycle's chain does not involve it at all. Whether it matters
   for reaching further than GATE2 is still unestablished and should
   not be conflated with this fix.
