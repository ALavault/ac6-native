# Correcting `ee1a401b` and `a1d0106e`: `CX360UnitManager` and the
# "render-state machine" are dead performance-counter instrumentation, not
# the render gate -- the real chain was already found and never merged in

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle for this report's own work (the cited chain used the
oracle's *executed-function set* only, per this campaign's standing rule).
Read of eleven prior commits' full messages, no source change.

## What happened

This session's own `ee1a401b` re-confirmed, via a properly-timed reachability
atlas, that `sub_820C2CC0` (the presumed seed-writer of `[0x827AD2F0]`),
`0x8217C4D8`, and `CX360UnitManager`'s seven constructor roots are unreached
-- and treated this as re-establishing `5dc58584`'s standing priority
("what constructs `CX360UnitManager`"). That duplicated `a1d0106e`
(2026-08-18, same method, same conclusion, already on `HEAD`) almost
exactly. **Neither commit noticed that the causal framing both were built
on -- `0x821ADAB8` arms rendering, `CX360UnitManager`'s absence is why the
frame is black -- had already been retracted**, by a separate lineage
(`12c5a372` -> `1a5c4f76` -> `b417938f` -> `7a86f4fb`) that is also an
ancestor of `HEAD` and was never cited by the thread `ee1a401b`/`a1d0106e`
continued. Two parallel, unreconciled readings of the same evidence have sat
in this history since 2026-08-18; this report joins them.

## The retraction, verified against primary sources (not taken on trust)

**`0x821ADAB8` is `D3D::CounterHandler`, not a `CX360UnitManager` event
callback.** Two independent lines of evidence, read directly this cycle:

- `12c5a372`: every `bctrl` in `sub_821ADC78` (the function that composes
  `0x821ADAB8`'s address) routes through `[[KeDebugMonitorData]]+24` or
  `[[KeCertMonitorData]]`, with `r3` = 28, 47, 64 -- debug-monitor
  notification codes, not game service numbers. The *only* site in the whole
  image that composes `0x821ADAB8`'s address is inside that debug-monitor
  call setup; no data table references it.
- `1a5c4f76`: Microsoft's own `d3d9.lib`/`d3d9i.lib` COFF archive symbols
  name `0x821ADAB8` as `D3D::CounterHandler(DWORD,DWORD)` at 14/24 matching
  windows, and `0x821ADC78` as `D3D::InitXBDMInterface(CDevice*)` at 3/24 --
  confirming `12c5a372`'s independent, code-read-only argument by name.
- `b417938f` later corrected `1a5c4f76`'s *unrelated* negative claims (that
  six other addresses were "Namco's code, not D3D's", based on a
  since-measured noise floor). It says explicitly: **"The conclusion that
  device+0x5460 is a performance-counter field stands. Only the sentence
  about Namco's code falls."** The `CounterHandler` identification is not
  among the retracted claims and is untouched by this correction.
- My own read of `sub_821ADAB8` this session (before I knew any of this)
  independently fits: it dispatches on a small code in `r3` (1, 16, 17, 34,
  224, 225...), sets/clears one bit via `slw`/`andc` keyed by `r4` at
  `device+0x12C`, zeroes an adjacent accumulator pair at `+21592`/`+21596`,
  and stores a raw value at `+21648`. That shape -- select counter N,
  enable/disable it, reset its accumulators -- reads naturally as a counter
  handler and awkwardly as a render-arm callback.

**`[0x827AD2F0]` is a performance-counter selector, not a render-readiness
state machine.** `7a86f4fb`, using a properly-calibrated window (32 bytes,
measured noise floor 0/40 on two certainly-native functions): `sub_821ACCD0`
-- one of the word's two writers -- is `D3D::GetCounter(CDevice*,
_D3DCOUNTER)` at 29/200 windows (d3d9.lib) and 8/200 (d3d9i.lib), against
the measured zero floor. `sub_821AD378`'s nine-case switch on this word,
which writes `[device+21508]`, is therefore a switch by counter type, not a
render-state dispatcher; `[device+21508]` is a counter field, same class as
`device+0x5460`. Quoting `7a86f4fb` directly: **"Two frontiers this campaign
chased turn out to be one thing: performance instrumentation that correctly
does nothing with no profiler attached."** `7a86f4fb` also explicitly
declines to call this the defect ("What I do not conclude is that this is
the defect... I am not replacing it with another here") -- the retraction
is of the *causal reading*, stated as carefully as the original claim was.

**Consequence for this session's own chain of reports.** Everything built
on "`0x821ADAB8` arms the render gate" or "`[0x827AD2F0]` must reach
`[11,19]` for rendering to start" -- `cf7116b2`, `03179c5b`, `8fba5b45`,
`ce065acb`, `3859edac`, `a1d0106e`, and this session's own `ee1a401b` --
measured real, correct facts (the reachability numbers are not in
question) but attached them to a mechanism that is very likely inert,
retail-normal instrumentation, not the cause of the black frame. The
`CX360UnitManager` framing specifically traces to `3e0c76d0`'s original
naming of `0x821ADAB8` as the mission-manager's event callback -- a naming
this cycle's evidence does not support.

## What is NOT retracted, and remains fact

- The reachability numbers themselves: `sub_820C2CC0`, `0x8217C4D8`,
  `CX360UnitManager`'s seven roots, `CX360MissionManager<...>`'s own
  constructors, `sub_821AD7C0`, and `sub_821ADAB8` are genuinely unreached
  on every route measured, including a correctly-timed START press
  (`ee1a401b`'s atlas, `a1d0106e`'s independent atlas). This is solid.
- `ece78019`'s own reading, previously the more cautious of the two: "if
  the absence of `CX360MissionManager<...>` is normal outside mission, or
  if it's already the lack" -- was left open there. This report resolves it
  toward **normal**: nothing in this evidence chain shows a mission manager
  should exist during the frontend/attract loop at all, and the
  `CounterHandler`/`GetCounter` identification gives an independent reason
  the whole `0x821AD...`-`0x821AC...` neighborhood is quiet (no profiler
  attached) that has nothing to do with mission state.

## The actual render chain, already found, sitting uncited in the same
## history (Fable-5 lineage, `b8482726` through `c2820200`)

Read and verified this cycle, in order:

1. `b8482726`/`b19fa6ef`: the object at `0x10041A00` is the XDK's own
   `D3DDevice` (confirmed via `VdSetGraphicsInterruptCallback`'s argument
   and the SDK's `d3d9.h` layout). The public struct is 10864 bytes
   (`0x2A70`); every offset this campaign has ever chased (10908, 10941,
   13216, 14872, 16536, 21508, 21600, 22264) lies in an undocumented
   private tail past the public struct's end -- library-symbol matching,
   not headers, is the only route to naming code there.
2. `1a5c4f76`/`b417938f` (calibrated): names `D3D::CDevice::KickOff`,
   `AddCallsToPrimaryBuffer`, `D3DDevice_Swap`, `D3D::CBlocker::Check`,
   `CreateInvalidateBuffer`, `BeginRingAlloc`, `D3D::SwapCallback` (24/60 in
   two archives, the strongest single ID), `InitXBDMInterface`.
3. `b93f52dd`/`92f76265`/`52c5d07c`: native reaches **zero** D3D functions
   the oracle doesn't -- a strict subset, not a divergence. `D3D::
   InitializeApiState` (oracle-executed, native-unreached) has no
   discoverable static caller at all.
4. `68936168`: same for `D3D::StartWorkerQueue`. All twelve `KeSetEvent`
   call sites in the whole image sit in functions unreached natively.
5. `c2820200` (closes the mechanism): `StartWorkerQueue` is never called by
   a CPU `bl` -- its address is composed and **written as data into the GPU
   command ring itself** (a type-3 packet, via `sub_821BAA78`/
   `sub_821BA1F8`, reached 23,504 / 35,367 times), exactly like every other
   ring word `sub_821C57D0` advances. It only runs once the GPU consumes
   that part of the buffer. Full chain: `WorkerThread` creation succeeds on
   the CPU side and blocks on an event only an advanced GPU sets;
   `StartWorkerQueue`'s address sits queued in the ring; **the ring's write
   pointer has not moved since tick 0**; the event is never set; the worker
   threads stay parked forever. "Not a second independent failure. Same
   lock -- the ring never kicked -- seen from a different frontier."
6. `5a7c3511`: the clearest single statement -- render-queue producer
   advances 7,495 times over 4,000 ticks (real game work, genuinely
   produced); the consumer advances zero times. 23 threads blocked, 0
   runnable. Explicitly leaves open which side of the cycle is upstream:
   "the ring does not advance so the events do not fire" and "the consumers
   do not advance so the ring is not filled" are two readings of one cycle
   this evidence does not separate.
7. `f6f44a5e`/`a568f40b`: the graphics/vblank interrupt callback **is**
   delivered, 12,001 times, once per tick, correctly -- ruling out the
   obvious "interrupt never fires" explanation. It has nothing new to
   signal because the GPU has crossed no new fence.
8. `e3b017db`: first genuine pixel check -- the front buffer is truly
   all-zero, and the oracle's own log confirms it never logs a mission
   either; this is a pure frontend problem on both native and oracle.

## Consequence for the plan

The campaign's standing priority is not "what constructs `CX360UnitManager`"
-- correcting `5dc58584` forward, six sessions later, on much stronger
evidence than either side of that debate had at the time. It is: **why does
the Xenos command ring never advance past its tick-0 boot state**, given
that the guest genuinely queues real work (`StartWorkerQueue`'s packet, and
7,495 producer advances of separate render-queue work) that a running GPU
would consume. `5a7c3511`'s open circularity (which side of ring-not-
advancing / events-not-firing is upstream) is the next question, and it has
a port-side half that does not require more guest archaeology: does this
port's own command-processor code (`graphics_ring.hpp`/
`xenos_command_processor.cpp`) actually read and execute past the tick-0
boot packets it already receives. A probe run earlier this session recorded
`ring: {read_pointer: 7, write_pointer: 25, submissions: 2,
pointer_mismatches: 2}` -- the consumer moved from 0 to 7 of 25 queued
dwords and then stopped, which is itself worth reading against
`xenos_command_processor.cpp`'s own packet-decode logic before writing any
more guest-side theories.

## Not established

- Which side of `5a7c3511`'s circularity is upstream.
- What, specifically, the port's command processor does or fails to do at
  dword 7 of the queued 25 -- not read this cycle.
- Whether `D3D::InitializeApiState` (the other callerless, oracle-executed,
  native-unreached function, explicitly *not* using the ring-as-data
  mechanism per `c2820200`) is on the same lock or a second one.
- The names of `sub_821AD378`, `sub_821AD7C0`, `0x821BE9A0`, `0x821C64E8` --
  none identified at a setting with a measured floor.

## Process note

This correction was caught by `advisor`, not by this session's own process:
a drafted report claiming a "shared dispatch table" between three functions
was independently falsified (it was `.pdata` metadata) before commit, and
that same caution prompted verifying this much larger claim against primary
sources (`12c5a372`, `1a5c4f76`, `b417938f`, `7a86f4fb`, `a1d0106e` read in
full) rather than trusting a research summary. `git log --oneline
<any-cited-commit>..HEAD` — checked this cycle and recorded in
[[forward-chain-checks-need-content-search]] as a standing lesson — would
have surfaced this entire lineage far earlier than discovering it by
accident mid-cycle.
