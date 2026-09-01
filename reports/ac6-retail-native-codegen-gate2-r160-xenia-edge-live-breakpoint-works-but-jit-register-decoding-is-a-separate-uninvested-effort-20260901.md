# AC6 retail NTSC-U/J — Xenia Edge's own live breakpoint mechanism reaches the exact `[r1+88]` guest instruction, but decoding its JIT register convention is a separate, uninvested effort (r160)

Date: 2026-09-01.

## Qualification

Oracle capture using the pinned Xenia Edge release (`60ff861`, r158's
verified install at `.tools/xenia-edge-60ff861/`), extracted
(`--appimage-extract`) and launched directly under `gdb` so its own `int3`
breakpoint mechanism traps into a real debugger rather than crashing. XEX/
ISO target unchanged from r157-r159 (retail NTSC-U/J,
`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`). No
files under version control modified; this report has no evidence
directory (no useful screenshots — the finding is register/log content,
already quoted below with hashes not needed since it is plain text).

## What was attempted

Following r159's correction (the retail binary's own code genuinely relies
on an uninitialized `[r1+88]` stack read; the open question is what
different environments' own execution histories leave there), this cycle
tried to get a *live* answer for what Xenia Edge's own execution leaves at
the equivalent point — not by guessing, but by using Xenia Edge's own
built-in CPU debug config option:

```
break_on_instruction = <guest address>   # int3 before the given guest address is executed.
```

Set to `0x823385d0` (decimal `2184414672`) — the exact guest address of the
`ld r31,0x58(r1)` instruction r159 verified via real disassembly. Launched
under `gdb --batch` with `run` + `info registers` + memory-examine
commands.

## What was found

**The breakpoint mechanism works exactly as intended** — genuine, verified,
reproducible:

```
Thread 41 "Guest CPU 0" received signal SIGTRAP, Trace/breakpoint trap.
0x00000000a01b10b1 in ?? ()
rax  0x7018f8b0   rbx  0x7018f8b0   rcx  0x823385d0   rdx  0x1
rsi  0x45e0000000 rdi  0x100000000 r8   0x821f7570   r9   0x70
r10  0x7018f800   r11  0x4         r12  0x8291053c   r13  0x82910530
```

This confirms, directly and unambiguously, that Xenia Edge's JIT genuinely
executes down to this exact guest PC (`rcx` holds the target address
`0x823385d0` verbatim, strongly suggesting it is used as a live comparison
value in Xenia's own breakpoint-check code) — the clearest possible
confirmation that this code path is really reached during a normal boot,
not skipped or dead in Xenia Edge's execution.

**Decoding the guest PPC register state from these host registers was not
achieved this cycle.** `rax`/`rbx`/`r10` (`0x7018f8xx`) fall in the same
numeric range as guest stack addresses this same log already reported
(`XThread... Stack: 70110000-70130000`, etc. — strongly suggestive these
are related to the guest stack pointer), but attempting to read guest
memory at those addresses directly via `gdb`'s `x` command failed
("Cannot access memory") — Xenia Edge's guest↔host memory mapping is not a
literal identity map at those addresses, at least not without whatever
base/PPCContext-pointer indirection its own JIT convention uses. `r12`/
`r13` (`0x8291053c`/`0x82910530`) *were* directly host-accessible and read
as all-zero — but their relationship to guest `r1` (if any) was not
established; they may simply be an unrelated, zeroed static/BSS structure
that happens to be host-mapped at its literal guest address.

## Decision

This is recorded as a genuine, bounded, partially-successful experiment,
not a dead end and not a full answer. It establishes the *mechanism* is
viable (Xenia Edge's own debug config can trap a live debugger at an exact
guest instruction) and that the code path is genuinely reached in a normal
boot — both useful, verified facts. It does **not** establish the actual
`[r1+88]` value Xenia Edge's execution leaves there, because decoding that
requires reverse-engineering Xenia Edge's own JIT register-allocation
convention (which host register or memory-mapped structure holds guest
`r1` at an arbitrary program point) from a stripped release binary with no
symbols — a separate, materially larger, open-ended investment whose
payoff is uncertain. Continuing to guess at register meanings without a
principled way to verify a guess would repeat exactly the "plausible rule
with no control" pattern this project's own discipline refuses (r111/r113's
precedent). This is where the DATA.TBL live-comparison sub-thread stops for
this cycle, per the same cost-benefit standard r144 and r154 already
applied to comparable open-ended detours.

## Gates

No native code changed, no build touched. `ctest`'s last-known state (9/9,
r155) stands unaffected. `git status` unchanged (only pre-existing,
unrelated dirty state). Neither this cycle's gdb sessions nor the earlier
Xenia Edge runs touched anything under version control.

## Next

1. If a future cycle wants to pursue this further, the concrete blocker is
   named precisely: find or derive Xenia Edge's JIT register/PPCContext
   convention (candidates worth checking first: whether `r13`/`r12`'s
   `+0xC` relationship recurs at other breakpoints, whether a debug/trace
   build with symbols is obtainable, or whether `store_all_context_values`
   + `trace_function_data`/`cpu_trace_mask` config options — seen in the
   full config dump captured this cycle — produce a more directly readable
   log-based trace instead of raw registers).
2. Absent that investment, the DATA.TBL sub-thread's status stands as r159
   left it: the retail binary's own code is faithfully translated (no
   XenonRecomp bug), the real game does not hang here in at least one
   working environment (r157/r158, qualified), and the exact reason a
   native fix would need to reproduce is not yet known.
3. Both of Gate 2's other named frontiers are unchanged: `sub_82390880`/
   `sub_821F5630` closed (r156); `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
