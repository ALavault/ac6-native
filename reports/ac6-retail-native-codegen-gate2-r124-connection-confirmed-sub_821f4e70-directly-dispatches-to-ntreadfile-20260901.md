# AC6 retail NTSC-U/J — connection confirmed live: `sub_821F4E70` dispatches directly to `NtReadFile`, and its caller explicitly expects `STATUS_PENDING` as a normal outcome (r124)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** One round of
`AC6_R124_DIAG`-gated instrumentation was added to and fully reverted
from the generated (gitignored, untracked) `ppc_recomp.27.cpp` --
confirmed via `grep -c "r124"` returning 0 and `ctest` 9/9 passing
afterward.

## Continuing r123's frontier: is `sub_821F4E70`'s indirect dispatch actually `NtReadFile`?

r123 named establishing whether `sub_821F4E70`'s indirect (`bctrl`)
dispatch reaches the file-open cluster r122/r123 examined -- a link
`FindDirectCallsTo.java` cannot see, since it only matches direct `bl`
instructions. Since the crash is deterministic (r116), one
instrumented run at the `bctrl` site settles this directly rather than
guessing from static analysis of a runtime-resolved function-pointer
table:

```
[r124] sub_821F4E70 dispatch target=0x823d035c
```

(printed six times, one per call -- matching r121's own count of six
`NtReadFile` invocations exactly).

**`0x823d035c` is `NtReadFile`'s own import-table address** (confirmed
in r122: `{ 0x823D035C, __imp__NtReadFile }`). This is not the
`Function_82390F48`/`Function_82391A40` cluster r122/r123 traced at
all -- it is a *direct* dispatch to the kernel import itself, a
separate, more central call site than the file-open cluster. r123's
named connection is answered: **the two threads of investigation are
not the same call site**, but `sub_821F4E70` is confirmed to be a
direct `NtReadFile` caller in its own right.

## The full register mapping, and why this caller explicitly expects `STATUS_PENDING`

Re-reading `sub_821F4E70`'s full body
(`generated/ppc_recomp.27.cpp` lines 1474-1625) with the confirmed
dispatch target maps every argument to `NtReadFile`'s real signature
(established in r122):

```
r3 = r30           (this function's own 1st argument -- FileHandle)
r4 = [r31+16]       (a field of the caller-supplied "I/O object" r31 -- Event)
r5 = 0                                                    (ApcRoutine)
r6 = 0, or r31 if bit0 of [r31+16] was set                (ApcContext)
r7 = r31            (the SAME I/O object -- &IoStatusBlock)
r8 = this function's 2nd argument (Buffer)
r9 = this function's 3rd argument (Length)
r10 = &(local copy of [r31+8]:[r31+12], a 64-bit value)   (&ByteOffset)
```

Immediately before the call, `r31 + 0` (the `IoStatusBlock.Status`
field) is explicitly set to `259` (`STATUS_PENDING`) --
`li r11,259; stw r11,0(r31)` -- the standard NT driver idiom for
"about to issue an async operation."

**After the call, the caller's own branch logic explicitly recognizes
three distinct outcomes**, not two:

```cpp
if (r3 < 0) goto handle_error_or_pending;      // genuine NT error
if (r3 == 259 /* STATUS_PENDING */) goto handle_error_or_pending; // ALSO here
// else: r3 == 0 (success) -- copies a result field and returns 1

handle_error_or_pending:
  if (r3 == 0xC0000011 /* STATUS_END_OF_FILE */) { ...; r3 = 0xC0000011; }
  else goto loc_821F4FE4;   // some other outcome (not traced further this cycle)
```

**This confirms, from the guest's own real logic rather than inference,
that `STATUS_PENDING` is an anticipated, explicitly-branched-on outcome
of this call** -- real hardware's async `NtReadFile` genuinely can
return 259 here, and the caller has dedicated handling for it. This
directly reinforces r109/r117/r119's whole "997/pending-status,
retry-then-give-up" reading of the surrounding state machine: this
specific call site is where that pending status would originate on
real hardware.

## What the harness actually returns here

Our harness's `NtReadFile` falls through to the generic offline stub
(`ctx.r3.u64 = kOfflineStatus = 0xC00000BB`, r121's finding). `0xC00000BB`
matches none of the three recognized outcomes above (`< 0` is true, so
it *does* enter the error/pending branch, but then fails the
`== 0xC0000011` check and falls into the unexamined `loc_821F4FE4`
branch) -- a fourth, generic-failure path this specific caller was not
obviously designed to expect as its everyday case.

## Decision

This closes r123's named connection question with a live, direct
answer rather than continued static inference: `sub_821F4E70` is
confirmed to call `NtReadFile` with a fully mapped, real-signature-
matching argument set, and its own caller-side logic independently
confirms this project's whole `997`/pending-status reading of the
retry loop is correct, not speculative. What remains open is narrower
than before: what `loc_821F4FE4` does with a *non*-`STATUS_END_OF_FILE`
negative result, and whether that path is where r119's per-thread
status field (`ctx.r13+336`'s indirection) actually gets written --
not yet traced.

No native code changed. Given real hardware's own design already
branches on `STATUS_PENDING` as a normal case, r123's "maybe the fix is
just a different failure status" framing looks less likely than
originally proposed -- a `kOfflineStatus`-shaped generic failure was
never one of this caller's anticipated outcomes either way. The
narrower, more promising fix candidate is now: have `NtReadFile`'s stub
return `259`/`STATUS_PENDING` on the *first* call (matching what real
async hardware would say immediately), then have something (a
short-lived timer, or the read genuinely completing synchronously
underneath) eventually update the `IoStatusBlock`'s real completion
status the way real hardware's DMA/interrupt completion would -- not
yet designed or verified this cycle.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`recompilation/ace-combat-6-retail/`: clean (instrumentation fully
reverted before this report, confirmed via `grep -c "r124"` returning
0).

## Next

1. Trace `loc_821F4FE4` (what happens to a non-`STATUS_END_OF_FILE`
   negative/unexpected result) to find whether it writes into the
   per-thread status field r119 traced (`ctx.r13+336`'s indirection),
   closing that remaining gap precisely.
2. Design (do not yet implement) what a correct `NtReadFile` stub
   needs to do given this caller's own explicit `STATUS_PENDING`
   handling: return `259` on first issue, and have the `IoStatusBlock`
   genuinely updated to a real completion status before the retry loop
   exhausts its counter -- this project's own `wait_event`/wait-with-
   bounded-retry infrastructure (`tools/materialize_native_import_stubs.py`)
   may already have a reusable pattern for "eventually becomes ready."
3. `Function_82390F48`'s cluster (r122/r123's fixed-offset
   hard-drive-partition read) remains a separate, still-unconnected
   thread -- not part of this specific `sub_821F4E70` call chain.
   Whether it matters for Gate 2 at all is still unestablished.
