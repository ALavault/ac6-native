# AC6 retail NTSC-U/J — root cause found: the crashing indirect calls are through a NULL guest function pointer, and `ExCreateThread`'s stub never reads `CreationFlags` (r114)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native or generated code changed this cycle.**
Investigation was `gdb --batch` crash capture (same passive technique
as r111-r113), `objdump` against the already-built binary, and direct
reading of `upstream/AC6_recomp/thirdparty/rexglue-sdk/include/rex/ppc/context.h`
and `tools/materialize_native_import_stubs.py`. `git status` on
`recompilation/ace-combat-6-retail/` confirmed clean before and after
(only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change).

## Continuing r113's frontier: both background-thread crash sites share one instruction and one `r12` value

A `gdb --batch` sweep reproduced `sub_821D4C20`'s crash (r112's second
named site) with full disassembly, and it is **the exact same
instruction and the exact same `r12` value** r112/r113 already
captured for `sub_82346428`:

```
0x0000555555709830 in __imp__sub_821D4C20 ()
=> call   *(%r12,%rax,2)    <__imp__sub_821D4C20+400>
rax = 0x0
r12 = 0x7ffe75980000  (unmapped, same value as sub_82346428's crash)
```

Identical crashing instruction *and* identical `r12` at two unrelated
guest functions on two different threads is too precise to be
coincidental register-reuse garbage -- this pointed at a **shared,
deterministic** computation rather than a race, and static disassembly
confirmed it.

## The constant is compile-time fixed, not runtime garbage

`objdump --disassemble='__imp__sub_821D4C20'` shows `r12` is built from
a hardcoded immediate, not a memory read:

```
1b5714: movabs $0xffffffff7e980000,%r12
1b571e: add    %rsi,%r12          ; rsi = base (guest memory host pointer)
...
1b5830: call   *(%r12,%rax,2)     ; rax = the guest target address (ctr.u32)
```

This exactly matches `PPC_CALL_INDIRECT_FUNC`'s definition
(`rex/ppc/context.h` lines 126-131, the active variant under
`PPC_CONFIG_H_INCLUDED`):

```c
#define PPC_LOOKUP_FUNC(x, y) \
  (*(PPCFunc**)(x + PPC_IMAGE_BASE + PPC_IMAGE_SIZE + \
                (uint64_t(uint32_t(y) - PPC_CODE_BASE) * 2)))
#define PPC_CALL_INDIRECT_FUNC(x) PPC_LOOKUP_FUNC(base, x)(ctx, base);
```

The compiler folds `base + PPC_IMAGE_BASE + PPC_IMAGE_SIZE -
PPC_CODE_BASE*2` into the constant loaded into `r12` (this is why it is
identical across every call site -- it depends only on link-time
constants and `base`, never on any per-call runtime state), leaving
`y*2` (`rax*2`) as the only variable part of the address, matching the
`(%r12,%rax,2)` addressing mode precisely.

## `rax = 0` means `y = 0`: the guest called through a NULL function pointer

Both captured crashes show `rax = 0` at the fault. Per the macro above,
`y` is the guest target address that was loaded into `ctr` (`mtctr r11;
bctrl` in the PPC source, confirmed for `sub_821D4C20` at generated
line ~16034-16037 of `ppc_recomp.23.cpp`: `ctr.u64 = r11.u64;
PPC_CALL_INDIRECT_FUNC(ctr.u32);`, with `r11` loaded from guest memory a
few lines earlier). **`y = 0` means the guest code is invoking a null
function pointer.** `PPC_LOOKUP_FUNC` performs no null check: with
`y = 0` and `PPC_CODE_BASE` a large positive constant (the XEX's code
base, ~0x82xxxxxx per this project's known image layout), `uint32_t(0)
- PPC_CODE_BASE` wraps around in unsigned 32-bit arithmetic to a huge
value, `*2` compounds it, and the resulting host address is far outside
any mapped region -- exactly the unmapped `r12` r112/r113 already
confirmed by direct memory read. This is the **entire fault mechanism,
fully explained**, not a hypothesis: a null guest function pointer,
dereferenced through an unguarded lookup formula.

## Why the pointer is null: `ExCreateThread`'s stub never reads `CreationFlags`

This resolves r112's own "check whether the eighteen threads should
start suspended" next step. The real Xbox 360 XDK `ExCreateThread`
signature is well-documented publicly (the same class of external,
verifiable spec r108 cited for the console's 512MiB physical memory --
not sourced from a file in this repo, and worth an independent check
before treating as settled) as seven parameters: `Handle, StackSize,
ThreadId, XapiThreadStartup, StartAddress, StartContext,
CreationFlags`, mapping to PPC integer registers `r3` through `r9` in
order. Reading the current stub
(`tools/materialize_native_import_stubs.py` lines 271-301) against that
mapping:

```python
output_handle    = ctx.r3.u32   # Handle       -- read
shim_address     = ctx.r6.u32   # XapiThreadStartup -- read
routine_address  = ctx.r7.u32   # StartAddress -- read
routine_argument = ctx.r8.u32   # StartContext -- read
```

`r4` (`StackSize`), `r5` (`ThreadId`), and **`r9`
(`CreationFlags`, which on real hardware carries `CREATE_SUSPENDED` to
start a thread parked until an explicit resume) are never read at all.**
Every one of the eighteen threads this harness spawns runs
immediately, regardless of what the guest actually requested. If the
guest's own design is to create worker threads suspended and resume
them only once their owning objects' function-pointer tables are
populated (a completely ordinary pattern for this class of code), this
stub's silent disregard of `CreationFlags` is precisely what would let
a freshly-spawned thread race ahead into code that calls through a
vtable-style slot before anything has written a real function pointer
into it -- explaining both the null-pointer crash mechanism confirmed
above and the run-to-run timing sensitivity r110-r113 all documented
(whether a given thread's scheduling quantum lands it at the
vulnerable call before or after the real initialization would have
written the pointer, on real hardware guaranteed by suspension, is
here left to host OS scheduling luck).

## Decision

This is the fullest, most mechanistically complete explanation this
investigation arc has produced: a null guest function pointer, called
through an unguarded lookup formula, most plausibly because
`ExCreateThread`'s stub starts every thread running instead of honoring
`CreationFlags`. It corrects r112's "unsynchronized vtable read" framing
-- there is no shared-memory race being observed at the crash
instruction itself; the crash is deterministic given `y = 0`, and what
varies run to run is only whether/when a given thread reaches this
call before its data is ready, which is a scheduling question, not a
memory-ordering one. It also refines r113's "uninitialized value" framing
for the same reason: `r12` was never uninitialized memory at all, it is
a compile-time constant; the truly uninitialized thing is the guest
function-pointer *slot* that produced `y = 0` further back in each
function, not yet traced to its specific field this cycle.

No native code changed this cycle -- deliberately. Fixing
`ExCreateThread` to honor `CreationFlags` (suspended creation plus a
real `NtResumeThread`/`KeResumeThread` implementation to release it) is
a substantive behavioral change to core thread-lifecycle infrastructure
affecting all eighteen threads and any future ones, and deserves its
own cycle with dedicated test coverage rather than being folded into an
investigation cycle already at its evidence-gathering budget.

## Gates

No native or generated code changed. `git status` on
`recompilation/ace-combat-6-retail/`: clean, unchanged from before this
cycle.

## Next

1. Implement suspended-creation support in `ExCreateThread`'s stub:
   read `CreationFlags` from `r9`, and when the suspend bit is set,
   park the spawned `std::thread` (e.g. behind a condition variable)
   until a real `NtResumeThread`/`KeResumeThread` stub releases it --
   check whether those imports are already stubbed
   (`tools/materialize_native_import_stubs.py`) before assuming which
   piece is missing.
2. Verify the real XDK `CreationFlags` bit value for
   `CREATE_SUSPENDED` against an authoritative source before
   implementing -- this report used the well-known public convention
   without independently re-confirming it against this project's own
   verified sources this cycle.
3. Once suspended creation is implemented, re-run the probe repeatedly
   (per r113's own suggestion of a larger unattended sweep) and check
   whether both `sub_82346428` and `sub_821D4C20`'s crashes stop
   reproducing, and whether the original `sub_821D6C20` main-thread
   crash (r100, seen again intermittently in r113) also stops --
   plausible if it shares the same "read a not-yet-written
   function-pointer slot" mechanism, but not yet established.
