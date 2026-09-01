# AC6 retail NTSC-U/J — r102's "one of five gates returns 0" hypothesis is wrong; the real divergence point is still open (r103)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle — live GDB tracing plus
static re-reading of r102's own evidence.

## Continuing r102's frontier, and correcting it

r102 named a bounded next step: identify which of five internal guard
calls inside `Function_821D5F48` fails and causes the shared bailout that
skips the singleton-construction site. This cycle attempted exactly that
and found r102's framing itself needs correction.

## What a live, filtered trace actually showed

Built a GDB breakpoint on each of the five gate functions
(`sub_82338300`, `sub_821F4078`, `sub_821CC508`, `sub_821D28C8`,
`sub_821D5600`), each conditioned on the caller's return address falling
inside `Function_821D5F48`'s own host code range (`__imp__sub_821D5F48`
to `__imp__sub_821D6C1C`, read from `nm`, avoiding the ambiguous-callee
problem r102 hit) — plus an unconditioned breakpoint on `sub_821D6C20`
itself to confirm the crash site is still reached. Run twice,
**reproducibly identical both times**:

```
GATE1 sub_82338300 called from parent
GATE2 sub_821F4078 called from parent
REACHED_CRASH_SITE sub_821D6C20
```

Gates 3, 4, 5 (`sub_821CC508`, `sub_821D28C8`, `sub_821D5600`) never fire,
on either run.

## The correction: gate 2's return value does not fail its own check

Captured gate 2's actual return value at this exact call site (a
temporary breakpoint at the return address, reading the guest `r3` field
of the same `PPCContext` pointer captured at entry — `PPCContext::r3` is
the second field, at byte offset 8, after `kernel_state`; confirmed from
`rex/ppc/context.h` and the `Register` union's layout in `rex/ppc/types.h`,
not guessed):

```
r3 = 0x8feffcb0
```

**Nonzero.** `Function_821D5F48`'s own check at this site
(`0x821d6170: or r31,r3,r3; 0x821d6174: cmplwi cr6,r31,0x0; 0x821d6178:
beq cr6,0x821d6138`) branches to the bailout only if the *full* r3 is
zero — it is not. So gate 2 passes its own check. r102's model (each of
five gates independently causes the shared bailout on its own failure) is
too simple: gate 1 and gate 2 both run and gate 2 does not fail by its own
criterion, yet gate 3 is never reached.

## Re-reading the block between gate 2 and gate 3 found no explanation

Re-examined every instruction between gate 2's check (`0x821d6178`) and
gate 3's call (`0x821d635c`) — about 115 instructions, several nested
calls (`sub_82383F0`, `sub_82221DD0` twice, `sub_82221F40` twice,
`sub_82222D80` three times, `sub_823D009C` twice, `sub_821CC288`,
`sub_821CC370` twice, `sub_821CC008`) and three small local
if/loop constructs, all of which converge back into the same linear flow
within this span. **No branch instruction in this range targets anywhere
outside it.** By this reading, `0x821d635c` should be reached
unconditionally once gate 2's block starts. It evidently is not, on two
reproducible live runs.

## An attempt to resolve this directly made things less clear, not more

Tried breaking on `sub_821CC508` (gate 3) alone, unconditioned, to see
whether it fires from *any* caller during the run. This did not complete
in 240 seconds — no hit, no crash, no further output beyond early thread
creation. That is different from "confirmed never called": this project's
own prior cycles (r100, r101) already documented that GDB-attached runs
of this probe behave very differently in timing from native runs (a
180-second attach in r100 never reproduced a crash that the native run hit
inside ~60 seconds), and this codebase has a known history of
timing-sensitive, racy multithreaded behavior (the busy-spin/blocking-wait
fixes of r90/r91). A single extra breakpoint changing which side of an
internal race is taken is a live possibility here, not a stretch — this
project should not assume a GDB timing artifact and a guest-code behavior
are the same thing without more evidence than a 240-second hang.

## Decision

Not asserting a specific mechanism for why gate 3 is unreached, and not
asserting r102's "gate returns 0" model applies to gate 3 either — that
would repeat the same mistake this cycle just caught in gate 2. What is
solid: the shared-bailout structural model from r101/r102 (one function,
two exits, a construction site reachable only past all five gate calls)
remains correct as *static* control-flow fact — `FindPpcAddressMaterialization.java`
and the full-function disassembly are unaffected by this correction. What
r102 got wrong was assuming *any* one gate's own local check must be the
mechanism; gate 2's actual runtime value refutes that for gate 2, and by
extension the model needs verification per-gate rather than assumed.

## Gates

No native code changed this cycle. `ctest`/pytest not re-run; `git
status` confirms no files touched.

## Next

1. Determine why `0x821d635c` (gate 3's call) is not reached despite no
   found static control-flow exit from the gate-2-to-gate-3 block. Two
   concrete options: (a) single-step (not breakpoint-trap) through this
   specific span on a live run to watch it directly rather than relying on
   breakpoint-hit absence, which this cycle showed can be misleading under
   this program's timing sensitivity; (b) re-examine whether one of the
   ~10 nested calls in that span (listed above) could itself not return in
   the conventional sense (a longjmp-style transfer, a thread hand-off, or
   a call that this specific native harness's stub implementation handles
   in a way real hardware wouldn't) — particularly worth checking against
   this project's own list of stubbed imports.
2. Once the real divergence point is found, re-attempt identifying
   whether it is gate-driven at all, or something else entirely (a stub
   behavior, a genuine race, or a static-analysis-invisible transfer) —
   without assuming the answer ahead of the evidence, per this project's
   standing discipline.
3. r100's older open thread (the `sub_821E6AC8`/`sub_821F03B0` wait chain,
   r94) stays probably moot for the same reason r101/r102 gave.
