# AC6 retail NTSC-U/J — `sub_821F4E70`'s `NtReadFile` call always uses `INVALID_HANDLE_VALUE`; `NtCreateFile` never produced a real handle in the first place (r127)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle -- one
read-only `FindPpcAddressMaterialization.java` check), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** One round of
`AC6_R127_DIAG`-gated instrumentation was added to and fully reverted
from the generated (gitignored, untracked) `ppc_recomp.22.cpp` --
confirmed via `grep -c "r127"` returning 0 and `ctest` 9/9 passing
afterward.

## Continuing r126's frontier: where does `sub_821F4E70`'s file handle come from?

r126 named tracing `sub_821F4E70`'s `FileHandle` argument (`r30`,
becomes `NtReadFile`'s `r3`, per r124's mapping) before designing that
import's fix. Reading the call site
(`generated/ppc_recomp.22.cpp`, both of the two places
`sub_821CC508` calls `sub_821F4E70`) shows the handle is not passed in
directly -- it is looked up from a small global table:

```cpp
r11 = sign_extend(PPC_LOAD_U8(r30 + 0));   // a "type" byte on the caller's state object
r11 = (r11 << 2) & 0xFFFFFFFC;              // *4, word index
ctx.r3.u64 = PPC_LOAD_U32(r11 + r24);       // r24 = a fixed global table base
sub_821F4E70(ctx, base);                    // ctx.r3 is now the "FileHandle" argument
```

## Live capture: the handle is always `0xFFFFFFFF`

Since the crash is deterministic (r116), one instrumented run at both
call sites was sufficient rather than a repeated sweep:

```
[r127] sub_821F4E70 call: type_byte=0 table_base=0x8293b93c table_index=0x00000000 handle=0xffffffff
(x6, matching r121's exact NtReadFile call count)
```

`type_byte = 0` at every call, so `table_index = 0` and the resolved
handle is always the table's very first entry, at guest address
`0x8293b93c`. **The value is `0xFFFFFFFF` -- `INVALID_HANDLE_VALUE`,
the standard, extremely well-known Win32/NT sentinel a real
`NtCreateFile`-family call (or the guest's own wrapper around it)
writes when handle creation fails.**

This is decisive on its own, independent of anything else this
investigation has traced: **`sub_821F4E70` calls `NtReadFile` with an
invalid handle on every single attempt.** No amount of correctly
implementing `NtReadFile`'s status/completion semantics (r125's
design constraint) can produce a real read against a handle that was
never valid to begin with -- the file this table entry is meant to
represent was never successfully opened.

## Confirmed this is a real, deliberate write, not zero-fill

`0xFFFFFFFF` is not consistent with "this address was never touched" --
this project's own `GuestAddressSpace` guarantees zero-fill-on-first-
touch (established early in this session's history), so an untouched
address would read `0x00000000`, not all-ones. Something in the guest
genuinely wrote `INVALID_HANDLE_VALUE` here, almost certainly as its
own "failed to open" bookkeeping after an earlier `NtCreateFile` call
(falling through to this harness's generic offline stub, r121's
finding) returned a failure status without ever writing a real handle
to its output parameter.

`FindPpcAddressMaterialization.java` (read-only) found no `lis`+`addi`/
`ori` pair building this exact address elsewhere in the XEX -- the
writer, wherever it is, computes the same table base through a
different code path (very plausibly the same `r24`-style base register
pattern seen throughout this session's tracing, materialized
independently in that other function) rather than re-deriving the
literal constant. Not yet located this cycle.

## Decision

This closes the remaining open question from r122/r123/r126 more
sharply than expected: the failure is not really about `NtReadFile`'s
own pending/completion semantics at all -- it is upstream of that,
at whichever `NtCreateFile` call is supposed to populate table entry 0
at `0x8293b93c`. `NtReadFile`'s fix (r125's own design constraint about
avoiding an infinite pending-loop) remains real and necessary, but it
is the *second* problem, not the first: even a perfectly-designed
`NtReadFile` would still fail immediately given an invalid handle,
before any of the `STATUS_PENDING`/retry machinery r109-r125 traced
ever gets a chance to matter.

No native code changed. This narrows, rather than closes, the next
step: find the specific `NtCreateFile` call that is supposed to write
a real handle to `0x8293b93c`, and whether it is the same
`Function_82390F48`/hard-drive-partition cluster r122/r123 traced (in
which case r123's "maybe this device genuinely doesn't exist on a
disc-based title" framing becomes directly relevant to *this* chain
too) or a third, still-unfound call site.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`recompilation/ace-combat-6-retail/`: clean (instrumentation fully
reverted before this report, confirmed via `grep -c "r127"` returning
0).

## Next

1. Find what writes `0x8293b93c`. `FindPpcAddressMaterialization.java`
   found nothing directly -- try `FindDataPointersTo.java` or a search
   for the same `r24`-style base-register materialization pattern
   (`lis`+`addi` near `-32108`/similar) across the XEX's other
   functions, since this session's own tracing shows that exact base
   pattern reused across multiple, separate functions.
2. Once found, determine whether it is `Function_82390F48`'s cluster
   (r122/r123 -- the fixed-offset hard-drive-partition probe) or a
   distinct `NtCreateFile` call this investigation has not yet located.
   If it is the partition probe, r123's "this may be expected to fail
   on a disc-based title" hypothesis becomes the operative question for
   *this* chain, not a separate concern.
3. Only once the real `NtCreateFile` call site and its expected
   real-hardware outcome (success with a real file, or an expected
   clean failure) are known should `NtReadFile`'s own fix be designed --
   attempting it before this is resolved risks fixing a symptom two
   layers downstream of the actual gap.
