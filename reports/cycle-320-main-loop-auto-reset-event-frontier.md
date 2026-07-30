# Cycle 320 — main-loop auto-reset event frontier

> **[ATTRIBUTION RESTORED BY CYCLE 323 — READ THIS FIRST]**
> Cycle 321 withdrew this cycle's `cross-match` attribution to POSIX auto-reset
> wake ownership, and cycle 322 dismissed its anomaly as an endianness reading
> error. **Both withdrawals are wrong.** Cycle 323 measured the full 64-bit
> value at `0x82870828` as `0` in *both* halves, so the anomaly this cycle
> reported is real, and reproduced the deadlock deterministically (3/3) in
> `scripts/ac6_condition_pingpong_regression.cpp`. The wake owner is the
> signalling thread itself, not a late waiter — a mechanism neither 320 nor 321
> named. This cycle's dynamic observation *and* its attribution to POSIX event
> wake ownership both stand. The specific SDK reading in §"SDK boundary" below
> is still imprecise: `Signal()` does release only one waiter, so it is not the
> "shared-boolean approximation" described there. See
> `reports/cycle-323-self-consumed-wake-and-contamination-sweep.md`.

## Target

- Product: AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module: `default.xex`
- Module SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Image base: `0x82000000`
- Native executable SHA-256:
  `2f95bf266848a198c964da2ded7da13367e34bf021c4ee754e553904f9530f3c`
- Route: `runtime-blocked`

## Result

The main loop is not blocked in `sub_8233AB00` before its condition check.
Direct stepping of its third invocation showed:

- the mutex acquisition through `sub_821F40E8` returned;
- the shared value at Xbox 360 PAL address `0x82870828` was `0`;
- the requested value was also `0`.

After 30 seconds of free execution, the exact stable frontier was instead the
wait loop inside `sub_82346108`. Two guest threads were sleeping on the same
host event object:

| Guest path | Requested value | Native wait |
| --- | ---: | --- |
| Main XThread: `sub_821D7D90 -> sub_82332360 -> sub_8233BA78 -> sub_8233AB00` | `0` | `NtSignalAndWaitForSingleObjectEx`, handle `0xF80000A8` |
| Worker: `sub_8233AD70` | `1` | `NtSignalAndWaitForSingleObjectEx`, handle `0xF80000A8` |

Both calls signal mutex handle `0xF80000A4` while waiting on event handle
`0xF80000A8`. GDB resolved both handles to the same host `XEvent` object
`0x7fff2800f380`; `manual_reset_` was `false`. At the stop, the shared guest
value remained `0`, so the main thread was the eligible waiter but remained
asleep. Confidence: `confirmed`.

## SDK boundary

The Xbox object is deliberately created as a synchronization event
(auto-reset). The POSIX SDK implementation represents it with one boolean:
`Signal()` sets the boolean and calls `notify_all()`, while the first waiter
that acquires the host mutex clears it. This permits the worker waiting for
value `1` to consume the notification intended for the already-sleeping main
waiter waiting for value `0`, then immediately sleep again. The Windows backend
uses native `SignalObjectAndWait`, which does not use this shared-boolean
approximation.

This semantic mismatch is the leading root-cause candidate for the frozen
guest frame loop. Confidence: `dynamic` for the two-waiter deadlock state;
`cross-match` for attribution to POSIX auto-reset wake ownership. A targeted
SDK regression test is still required before changing the runtime.

## Validation

- Direct GDB stepping reached and returned from the third main-thread
  `sub_821F40E8` mutex acquisition.
- A 30-second free run was interrupted and all native thread stacks inspected.
- Main thread stack:
  `sub_821F5828 -> sub_82346108 -> sub_8233BA78 -> sub_82332360 -> sub_821D7D90`.
- Worker stack:
  `sub_821F5828 -> sub_82346108 -> sub_8233AD70`.
- Both stacks resolved the same event handle, same host `XEvent`, and
  auto-reset mode.

No generated output or native source was changed in this cycle. This is a
runtime synchronization frontier, not playability or retail-parity evidence.

## Next question

Add the smallest SDK-level regression that proves an already-selected
auto-reset-event waiter cannot have its signal stolen by a later waiter, then
correct the POSIX event implementation and rerun the AC6 frame-loop probe.
