# The D3D ring doorbell is not kicked past tick 0 because `[0x827AD2F0]` never leaves 0

## Qualification

- Target: AC6 Xbox LIVE demo (PAL), `Default.xex` SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Guest project: `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
- Instrument: `recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe`,
  `--input-at 252,16,0,0,0,0,0,0,1`, `--max-ticks 5600`, backend `vulkan`,
  `SDL_AUDIODRIVER=dummy`, no oracle.
- Generated code read: `codegen/generated/ppc_recomp.20.cpp` (`sub_821B9BC8`),
  `codegen/generated/ppc_recomp.18.cpp` (`sub_821AD378`). Both functions were
  read in full for this report; no offset or constant below is inferred from a
  name.

## The question this closes

`AC6_DEMO_STARTWORKERQUEUE_IS_QUEUED_ON_THE_GPU_NOT_CALLED.md` (`c2820200`)
traced `D3D::StartWorkerQueue`'s absence back to "the ring's write pointer has
not advanced since tick 0" without naming why. This report names why, with a
control on every step: static PPC reads plus a live trace with
`AC6_DEMO_WATCH_RING_KICK=1 AC6_DEMO_WATCH_MODE_STATE=1`.

## The doorbell function, read

The MMIO write at `0x7FC80714` (the ring write-pointer register,
`recompilation/ace-combat-6-demo/src/guest_bridge/lifecycle.hpp:77-93`) is
issued from exactly one place in the generated code: `sub_821B9BC8`, at
`loc_821B9D4C` (`stw r29,1812(r11)` with `r11 = 0x7FC80000`, confirmed by
`lis r11,32712` immediately above it).

`sub_821B9BC8(device, r30, count)` has two independent routes to that store:

- **Path A** (device byte `+10941` bit `0x02` clear): calls `sub_821B94A8`,
  then if `count == 0` branches straight to the doorbell store — no
  dependency on any other field.
- **Path B** (bit `0x02` set): loops writing `count` packets from `r30`,
  gated first on `device+21508 != 0` (bails to a no-op return otherwise) and
  again on `count != 0`; only falls through to the doorbell store after the
  loop completes.

## The measurement

Same run, `buttons16` route, `AC6_RING_KICK` and `AC6_DEVFLAGS` output:

```
AC6_RING_KICK tick=0 thread=1 lr=0x821C4A28 wptr 0x00000000 -> 0x00000016
AC6_RING_KICK tick=0 thread=1 lr=0x821C4A28 wptr 0x00000016 -> 0x00000019
AC6_RING_KICK tick=0 thread=1 lr=0x821C5C98 wptr 0x00000000 -> 0x00000016
AC6_RING_KICK tick=0 thread=1 lr=0x821C5C98 wptr 0x00000016 -> 0x00000019
AC6_DEVFLAGS tick=222  byte10941=0x06 bit1=1 f21508=0x00000000
AC6_DEVFLAGS tick=2000 byte10941=0x06 bit1=1 f21508=0x00000000
AC6_DEVFLAGS tick=4000 byte10941=0x06 bit1=1 f21508=0x00000000
```

All 4 kicks are at tick 0, before the trace's first `AC6_DEVFLAGS` sample —
consistent with bit `0x02` still being clear at device construction, i.e.
**Path A**, whose count-0 branch has no dependency on `device+21508`. From
tick 222 onward, bit `0x02` is set (`byte10941=0x06`), so every subsequent
call to `sub_821B9BC8` takes **Path B**, and `device+21508` is `0` for the
entire remaining 5600-tick run: Path B's gate is permanently false, and the
doorbell is never written again. This matches the standing finding
(`render_queue.consumer_changes = 0`) exactly.

## Why `device+21508` never becomes nonzero — the writer, read

`sub_821AD378` is the only function in the reachable set that stores to
`device+21508` (`stw r11,21508(r29)`, two sites, `ppc_recomp.18.cpp`). Both
sites are inside a 9-way `switch` whose selector is computed as:

```
r11 = [0x827AD2F0] - 11
if (r11 as unsigned > 8) goto loc_821AD73C   // bail, no write
switch (r11) { case 0..8: ... }
```

i.e. the switch only does anything for `[0x827AD2F0]` in `11..19`. Verified
independently this cycle by evaluating the address arithmetic
(`lis -32133` → `0x827B0000`; `+ -11832` → `0x827AD1C8`; `+296` →
`0x827AD2F0` — matches the address `trace_device_flags` already reads at
`frontend_state_trace.hpp:81,95`). The two `device+21508` writer sites sit in
the case-1 (`loc_821AD508`, selector `== 12`) and case-3 (`loc_821AD414`,
selector `== 14`) blocks.

`AC6_DISPLAYMODE` in the same run: `[0x827AD2F0]=0` at every sample
(`case=-11`) — outside `11..19` for the whole 5600-tick run, so
`sub_821AD378` bails at its first check every time it is reached and never
executes any case. `device+21508` therefore never moves off whatever it was
initialized to (0, per the first `AC6_DEVFLAGS` sample), because its one
writer is permanently gated shut.

## Discriminating result

Per the two hypotheses this cycle set out to separate: **the guard is
false** — `device+21508` is a field nothing in the reachable guest code
currently causes to be set to nonzero, not a case of the MMIO write handler
silently dropping a store that did execute. The MMIO handler is not
implicated by this measurement.

## Side finding: worker-thread creation is real, not a reachability artifact

Same report's `scheduler.waits[]`: 2 of 23 threads carry `wait_lr =
0x821C4A28`, the address immediately after `WorkerThread`'s first
`sub_821B9BC8` call — the address that appears twice in the kick log above.
Two independently-created threads parked at the exact instruction after that
call is strong evidence the thread-creation chain
(`sub_821C4E40`→`sub_821A6B38`→`sub_821A8CB8`, `c2820200`) produces working
threads that actually execute `WorkerThread`'s body, not threads that
silently trap and vanish from the runnable set for an unrelated reason.

## What is still open

Why the guest never drives `[0x827AD2F0]` into `11..19` is not established
here. `7a86f4fb` names that address as `D3D::GetCounter`'s performance-counter
selector, a correction of `5da91f72`'s "display mode" reading; this cycle's
own trace tag (`AC6_DISPLAYMODE`) predates that correction and is stale
naming, not a live claim about the field's role — the numeric evidence above
does not depend on which reading is right. The next control needed is: what
reachable code writes `[0x827AD2F0]`, if anything, and under what condition
it would write 12 or 14.

## Gates

`ctest` (24, demo) and the native mission01 gate (JF) were run before commit;
no source file changed in this cycle, only this report, so no rebuild was
required. `git status` after `ctest` showed only this report as new,
pre-existing untracked scratch artifacts under `analysis/demo/` unrelated to
this cycle's work, and the pre-existing uncommitted `AGENTS.md` fix from the
prior cycle (left as the user specified: fix applied, not committed).
