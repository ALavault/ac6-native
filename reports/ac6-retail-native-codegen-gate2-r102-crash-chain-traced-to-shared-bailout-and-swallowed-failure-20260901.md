# AC6 retail NTSC-U/J — the crash's causal chain is complete: a shared init bailout the caller doesn't honor (r102)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle — static trace plus a live
GDB breakpoint attempt (partially inconclusive, reported honestly below).

## Continuing r101's frontier

r101 found the exact global (`0x82935d98`) `sub_821D6C20` dereferences
null, found its one real construction site, and confirmed `sub_821D7DE0`
(the crash's sole caller of `sub_821D6C20`) doesn't visibly reach that
site — without yet identifying the function that *contains* the
construction site, or why it's skipped.

## The construction site's containing function, and its complete control flow

`GetFuncBounds.java` (a throwaway script querying Ghidra's own function
model, deleted after use): the construction block r101 found belongs to
**`Function_821D5F48`** (`0x821d5f48`-`0x821d6c1b`, 3283 bytes, ~820
instructions) — and `0x821d5f48` is exactly the address `sub_821D7DE0`
calls first (`0x821d7dec: bl 0x821d5f48`), confirmed against r100's own
disassembly of `sub_821D7DE0`. So the construction site **is** inside code
`sub_821D7DE0` calls; r101's "never reaches it" was imprecise — it reaches
the containing function, not necessarily the specific block inside it.

Dumped `Function_821D5F48` in full (`DumpRange.java`, 822 lines). It has
**no `blr`** anywhere in its body — real returns are shared-epilogue
tail-branches, both to `0x82382a48`. Searching for those found **exactly
two exits**:

```
0x821d6140  b 0x82382a48    ; early bailout -- li r3,0x0 immediately before
0x821d6c18  b 0x82382a48    ; normal end, past the singleton construction
```

The early-bailout label `0x821d6138` (`li r3,0x0` then fall into the exit
branch) is the **shared failure target of five separate guard checks**
scattered through the function, each following a different subsystem
`bl`:

```
0x821d6130-34  cmpwi cr6,r3,0 / bge cr6,+  (guards bl 0x82338300)
0x821d6178     beq cr6,0x821d6138          (guards bl 0x821f4078)
0x821d6368     blt cr6,0x821d6138          (guards bl 0x821cc508)
0x821d6708     blt cr6,0x821d6138          (guards bl 0x821d28c8)
0x821d6a48     beq cr6,0x821d6138          (guards bl 0x821d5600)
```

Any one of these five subsystem calls failing sends the function straight
to the shared bailout with `r3 = 0`, skipping everything after it —
including the singleton construction at `0x821d6be8`, which is only
reachable by falling through **all five** checks successfully.

## The caller doesn't honor the failure it explicitly checks for

`sub_821D7DE0`'s own disassembly (dumped in full by r100, re-examined
here):

```
821d7dec  bl 0x821d5f48
821d7df0  rlwinm r11,r3,0x0,0x18,0x1f   ; low byte of the return value
821d7df4  cmplwi cr6,r11,0x0
821d7df8  bne cr6,0x821d7e0c            ; success: skip the diagnostic
821d7dfc-821d7e08                       ; failure only: call sub_821F5B18
821d7e0c  bl 0x82331de8                 ; BOTH paths land here
   ...
821d7e40  bl 0x821d6c20                 ; reached unconditionally either way
```

The caller **does** distinguish success from failure — it calls
`sub_821F5B18` (plausibly a diagnostic/assert/log helper, not yet traced)
only on failure — but **does not branch away or return** on that path. It
falls through to the same continuation as success, ending at the call to
`sub_821D6C20` regardless. This is not ambiguous: there is no other edge
into `0x821d7e0c` from anywhere else in this function's body that would
explain a legitimate reconvergence; it is a direct fallthrough from the
failure branch.

## The chain is now complete

1. `0x82935d98` is genuinely null at the crash (r101, confirmed from the
   crash's own core dump, not just the static image).
2. Its one writer (`0x821d6be8`) is reachable only after five internal
   subsystem-init calls all succeed.
3. `Function_821D5F48` has exactly two exits: that writer's continuation,
   or a shared bailout (`r3=0`) taken if *any* of the five fail.
4. Its caller (`sub_821D7DE0`) checks the return value, distinguishes the
   failure case enough to call a diagnostic function, but proceeds to call
   `sub_821D6C20` on both outcomes.

This fully explains the crash mechanically, without guessing which of the
five gates actually failed or why `sub_821F5B18` doesn't stop execution.

## What this cycle could not establish, and why

Attempted to identify *which* of the five gates fails, live. Breaking on
each gate function's entry directly is ambiguous — `sub_821F4078`, for
instance, hit from an unrelated call site (`sub_823CBEE8`) before ever
reaching `Function_821D5F48`'s call to it, since these are ordinary
subroutines reused elsewhere in the image. Breaking on
`sub_821D5F48` itself (unambiguous: `FindDirectCallsTo.java` confirms
exactly one caller) and using `finish` to capture its return value timed
out at 90 seconds under GDB — plausibly `ptrace` single-step overhead
across ~800 instructions and however many nested calls the five gates
make, not evidence the function itself hangs (the crash trace already
proves it returns and execution reaches `sub_821D6C20` afterward, well
within the native, non-`gdb` probe's ~60-second run). **Not asserting**
which gate fails; this needs a targeted approach next cycle (e.g., a
purpose-built breakpoint at the caller-side check instructions via
computed offsets, or instrumenting the harness itself rather than GDB
`finish`), not a guess.

One direction worth naming without asserting it as established: if
`sub_821F5B18` is a fatal diagnostic/assert path on real hardware (halts
or raises rather than returning), a native import stub underneath it that
merely no-ops and returns would produce exactly this shape — the guest's
own "log and abort" intent silently becomes "log and continue" under this
harness. This is a hypothesis to check, not a finding.

## Gates

No native code changed this cycle. `ctest`/pytest not re-run; `git
status` confirms no files touched.

## Next

1. Identify which of the five gates (`sub_82338300`, `sub_821F4078`,
   `sub_821CC508`, `sub_821D28C8`, `sub_821D5600`) fails on this call
   path specifically, using a method that avoids the ambiguous-callee and
   slow-`finish` problems this cycle hit — a purpose-built breakpoint at
   the caller-side comparison instructions (computed via a small
   disassembly-driven offset, not guessed) is the most direct route.
2. Trace `sub_821F5B18` to determine whether it is a diagnostic-only path
   or a real-hardware-fatal one whose native stub silently downgrades it
   — this bears directly on whether "fix `sub_821D7DE0` to actually abort
   on failure" or "fix whichever gate import is stubbed" is the right
   layer to intervene at, once one is chosen.
3. r100's older open thread (the `sub_821E6AC8`/`sub_821F03B0` wait chain,
   r94) stays probably moot for the same reason as r101 gave.
