# AC6 retail NTSC-U/J — GDB live tracing of this probe is unreliable; both static readings agree gate 3 is unconditionally reached (r104)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle — static host-code
disassembly plus four further live GDB sessions, all reported honestly,
including the ones that didn't converge.

## Continuing r103's frontier

r103 corrected r102's "any one gate returns 0" hypothesis (gate 2's real
return value is nonzero) and left open why gate 3 (`sub_821CC508`) was
never observed live despite no found static skip in the guest PPC
disassembly. It recommended checking the *compiled host code*, not just
the guest reading, as one path forward.

## The host disassembly agrees with the guest disassembly: no skip exists

Captured gate 2's return address live and disassembled 220 host
instructions from there (`x/220i`, a static read, not a stepped
execution). The compiled x86 exactly mirrors the guest PPC reading r103
already did:

```
je   ...+6524     ; gate 2's own check (confirmed non-taken: r103's r3 read)
call sub_823830F0
call sub_82221DD0
call sub_82221F40
call sub_82222D80
je   ...+1409                       ; local if, both sides converge
call sub_82221DD0
call sub_82222D80
call sub_82221F40
call RtlInitializeCriticalSection   ; a native import stub, not a sub_XXXXXXXX
call sub_821CC288
call sub_821CC370
call sub_821CC370
call RtlInitializeCriticalSection   ; called a second time
jb / ja  ...+2157                   ; local if, both sides converge
call sub_821CC008
call sub_82222D80
call sub_821CC508                   ; <-- gate 3, unconditionally reached here
je   ...+2288                       ; gate 3's own check (retry loop)
js   ...+6524                       ; gate 3's own failure branch
```

**No conditional branch anywhere in this span leaves it before the call to
`sub_821CC508`.** This confirms, at the compiled-code level (not just the
generated-source level, which could in principle diverge from what's
actually emitted), that gate 3 is unconditionally called once gate 2's
checks pass. One new fact this reading surfaced that the guest-only trace
didn't show as clearly: `RtlInitializeCriticalSection` — a genuine kernel
primitive, not a generated guest function — is called **twice** in this
span. Worth keeping in mind for a future cycle (noted, not chased here).

Also checked and ruled out a concrete alternative explanation for the
earlier live no-hit: symbol ambiguity. `sub_821CC508` and
`__imp__sub_821CC508` resolve to the identical address (`0x1a4bd0`) —
GDB was not silently breaking at the wrong location.

## Four live GDB sessions, reported without cherry-picking

In order, across this cycle and r103 combined:

1. `gates2.gdb` (5 conditional gate breakpoints + crash-site breakpoint):
   completed twice, reproducibly, gates 1-2 hit, 3-5 did not, crash site
   reached both times.
2. `gate3_check.gdb` (unconditioned single breakpoint on gate 3 alone):
   hung 240 seconds, no hit, no crash, no completion.
3. `gates3.gdb` (all of the above plus one more breakpoint at a *fixed*
   host address left over from a prior session): a genuine scripting
   mistake — PIE relocates the load base differently per run, so a raw
   address from one session is meaningless in the next. Hung after gate 1
   only. Not evidence of anything about the guest program; a bug in this
   cycle's own script, corrected before drawing conclusions from it.
4. `gates4.gdb` (the corrected version of (3), same breakpoints as (1)
   minus the address bug): hung after gate 1 only, this time not even
   reaching gate 2 — a **third distinct outcome** from what is
   structurally close to the same script.

Four sessions, three different observed outcomes, from scripts that
should be behaviorally near-identical. That spread is itself the
finding: **GDB's ptrace-based tracing measurably changes this specific
probe's execution outcome, run to run**, on top of the timing-sensitivity
this project already documented in r90/r91/r100/r101. This is not a new
discovery about the guest program — it's a instrument-reliability finding
about the debugging method, in the same spirit as this project's own
"measure the instrument before trusting it" discipline (`CLAUDE.md`,
citing the four-version scan-counting history).

## Decision

Stop using live GDB breakpoint tracing to isolate control flow inside
this specific function on this specific probe. It has now produced three
different outcomes across four attempts without a script bug common to
all of them, which is disqualifying for a technique meant to establish a
fact. The **static** evidence — guest PPC and compiled host x86,
independently, agreeing with each other — is the reliable signal here:
there is no control-flow skip between gate 2 and gate 3.

This means r103's open question ("why does gate 3 never fire live") is
best explained as a GDB artifact, not a real behavior of the crash path.
r102's original structural finding stands: `Function_821D5F48` runs
linearly through all five gates when nothing fails, and the shared
bailout is reachable only by an explicit failure at one of them. Which
gate (if any) actually fails on this specific probe run remains
unestablished — this cycle neither confirms nor refutes that a gate
fails at all; it only retracts the specific, unreliable evidence that
seemed to point at gate 3.

## Gates

No native code changed this cycle. `ctest`/pytest not re-run; `git
status` confirms no files touched.

## Next

1. **Do not continue live GDB breakpoint tracing on this probe** for this
   question — it has been shown unreliable across independent attempts.
   A future cycle wanting to observe this control flow directly should
   use either the established reliable method (a bounded native run to
   completion, then `apport` core-dump inspection, as in r100/r101) or a
   throwaway instrumentation build: the generated `ppc_recomp` source for
   `Function_821D5F48` sits in the build tree as ordinary C++ and could
   carry a temporary diagnostic print for one run, reverted before any
   commit — not yet attempted, and worth trying before another live-GDB
   pass.
2. `RtlInitializeCriticalSection` being called twice in this exact span is
   a loose thread worth checking against this project's own native import
   stub table — a real behavioral difference from what real hardware does
   is a plausible place for a swallowed failure to originate, more so than
   guessing at guest-code control flow this cycle has now twice shown is
   not where the divergence is.
3. r100's older open thread (the `sub_821E6AC8`/`sub_821F03B0` wait chain,
   r94) stays probably moot for the same reason r101/r102/r103 gave.
