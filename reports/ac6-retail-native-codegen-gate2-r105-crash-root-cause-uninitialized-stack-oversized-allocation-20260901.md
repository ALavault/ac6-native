# AC6 retail NTSC-U/J — root cause found: an uninitialized stack slot drives an oversized MmAllocatePhysicalMemoryEx request (r105)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed** — this cycle's diagnostic edits
were made only in the build-tree's generated `ppc_recomp.*.cpp` files
(never hand-maintained source, gitignored, always rebuilt from the
codegen receipt), used for exactly two native runs, then reverted from a
backup before writing this report. `ctest` (9/9) reconfirmed clean
afterward; `git status` on `native/` shows nothing changed.

## Continuing r104's frontier

r104 retired live GDB breakpoint tracing for this question as unreliable
(three different outcomes across four attempts) and recommended, as an
alternative, instrumenting the generated `ppc_recomp` source directly for
one throwaway diagnostic run — untested at the time. This cycle did
exactly that.

## Method: instrument the generated source, run natively, revert

Copied `ppc_recomp.23.cpp` (containing `Function_821D5F48`) to a backup,
then added `fprintf(stderr, ...)`, gated behind a new env var
(`AC6_R105_DIAG`), at: the shared bailout label, the success-path return,
and each of the four explicit `goto`-to-bailout sites (tagging which gate
failed). Rebuilt `ac6recomp` and ran the ordinary bounded probe —
**no GDB, no `ptrace`, the exact same code path a normal build would take
in production** — with `stdbuf -oL -eL` to avoid losing buffered output
to the timeout kill (the first attempt without it produced zero bytes of
output for this same reason, worth noting for future diagnostic runs).

## Result: gate 2 (`sub_821F4078`) genuinely fails, reproducibly

```
[r105] gate2 (sub_821F4078) FAILED
[r105] Function_821D5F48 BAILOUT taken
```

Reproduced twice, identically, both ending in the same `SIGSEGV` r100
originally found. **This directly retracts r103's specific evidence**
(a live GDB read showing gate 2's return value as nonzero,
`0x8feffcb0`) — r104 already flagged GDB as unreliable for this probe in
general terms; this is the concrete case where that unreliability
produced a wrong answer. r102's original instinct (some gate fails) was
correct; r103's correction of *that* instinct was itself the artifact.

## Tracing gate 2 to its real failure: an oversized physical allocation

`sub_821F4078`'s generated C++ (not just its PPC disassembly) shows its
`bl 0x823d012c` is XenonRecomp's own resolved name for a **kernel
import**, not a `sub_XXXXXXXX` guest function:

```cpp
__imp__MmAllocatePhysicalMemoryEx(ctx, base);
mr. r31,r3
bne 0x821f40e4      // r3 != 0: success, return it
li r3,8; bl sub_821F7530   // r3 == 0: call an error helper, then return null anyway
```

Instrumenting this call site's arguments and return value directly
(same technique, same run) isolated the exact failing request among
seventeen total `MmAllocatePhysicalMemoryEx` calls made during this
probe (all sixteen prior ones succeed at normal sizes up to 2 MB):

```
[r105] MmAllocatePhysicalMemoryEx args r3=0x0 r4=0xffc00000 r5=0x20000004 r6=0x0 r7=0xffffffff r8=0x0
[r105] MmAllocatePhysicalMemoryEx returned r3=0x0
```

`r4` (the size argument, per this project's own already-verified stub
implementation in `materialize_native_import_stubs.py`) is
**`0xffc00000`** — as a signed 32-bit value, **`-0x400000` (-4 MiB)**,
wrapped to an unsigned ~4.09 GiB request that the guest heap's
`allocate_guest()` correctly refuses.

## The size's origin: an uninitialized stack slot

Traced backward through `Function_821D5F48`'s own code
(`0x821d61a8`-`0x821d61c8`, immediately before the `sub_821F4078` call):

```
lwz r11,108(r1)          ; r11 = a LOCAL STACK SLOT, offset 108 in this frame
lis r6,0x2000; ori r6,r6,4
li r5,0
li r4,-1
addis r30,r11,-0x800000  ; r30 = r11 - 8 MiB   <-- the underflow site
mr r3,r30
bl sub_821F4078            ; sub_821F4078's own body does `mr r4,r3` before the import call
```

`0x400000` (4 MiB) `- 0x800000` (8 MiB) `= 0xFFC00000` in 32-bit
arithmetic — exactly the observed request. So `r11` (the stack slot at
`ctx.r1.u32 + 108`) held **`0x400000`** at the moment of this read.

**Grepped `Function_821D5F48`'s entire generated body (1700+ lines,
`ctx.r1.u32 + 108`) for any write to this offset: none exists.** Checked
its one caller, `sub_821D7DE0` (confirmed sole caller via
`FindDirectCallsTo.java` in r102/r103): its first two instructions are
`mfspr r12,LR` and its own `stwu` prologue, then straight into
`bl sub_821D5F48` — it writes nothing to this stack region either.

**This offset is read as a local variable but never written anywhere in
the traced call chain.** It is uninitialized stack memory in this
harness's execution of this path. Whatever value real hardware would have
left there — from an entirely different call history than this native
harness reconstructs — this harness's stack happens to contain `0x400000`
here, which is 4 MiB too small for whatever the guest code assumes
(apparently a value of at least 8 MiB), producing the underflow.

## Decision

Not implementing a fix this cycle. Two genuinely different explanations
fit the evidence and this cycle cannot distinguish them without further
work, and guessing which would repeat exactly the mistake this project's
own discipline exists to prevent (`CLAUDE.md`: "a plausible rule with no
control is refused"):

1. This stack slot is a **hidden calling-convention dependency** — real
   hardware's boot sequence happens to leave a consistent, large-enough
   value there through some fixed prior call history this project hasn't
   traced, and the native harness's different call history (or its
   single-threaded, entry-probe-only boot path) produces a different,
   smaller leftover value. If so, the fix is finding and replicating
   whatever real sequence writes an appropriate value into that physical
   stack region before this point — not zero-filling or synthesizing one.
2. This is a genuine, narrower gap in *this* harness's stack/thread setup
   specifically (`initialize_probe_thread`'s minimal PCR/TLS bring-up, or
   the single-guest-thread model already flagged as a limitation in
   r101/r102) that a fuller boot sequence would not exhibit.

Either way, writing a synthetic value into that stack slot to make this
one call succeed would be exactly the kind of guess this project's
evidence discipline refuses — it would "fix" one allocation without
establishing whether the value used is actually correct, and could mask
a real gap instead of closing it.

## Gates

`ctest` (native profile): **9/9**, confirmed after reverting all
diagnostic changes. `git status` on `native/`: clean, nothing changed.
Full mission01/contract-artifact gates not re-run this cycle (no tracked
source changed); the pre-existing, unrelated N2 mismatch from r90-r104 is
untouched regardless.

## Next

1. Find what, in a fuller (non-probe) boot sequence or on real hardware,
   is expected to occupy the stack region at this offset before
   `Function_821D5F48` runs — likely requires either tracing further back
   through `_xstart`'s own earlier calls for anything that could leave a
   residual value at this physical stack location, or accepting this as
   a probe-harness limitation (single guest thread, no fuller subsystem
   bring-up) and scoping a fix to that specifically.
2. Confirm whether `r5=0x20000004` (the flags argument, unusually large —
   bit 0x20000000 set) on this specific failing call is itself unusual
   compared to the sixteen successful calls (several of which also carry
   `0x20000xxx`-prefixed flags, so this alone is likely not anomalous —
   worth a quick check, not assumed).
3. This cycle establishes *why* the crash happens with much more
   precision than r100-r104's structural findings — but does not resolve
   it. Do not claim the gate/singleton/crash chain is fixed; only that
   its root mechanism (not just its shape) is now known with reproducible
   evidence.
4. The build-tree-instrumentation technique used here (temporary
   `fprintf` in generated `ppc_recomp.*.cpp`, gated by an env var, always
   reverted before commit) is a legitimate diagnostic tool this project
   has not used before r105 and should be preferred over live GDB
   tracing for future control-flow/value questions on this probe, per
   r104's finding that GDB itself is unreliable here.
