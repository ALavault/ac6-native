# AC6 retail NTSC-U/J — the post-IB stall is a counting fence, decrement site not yet found (r77)

Date: 2026-08-31.

## Question

r51/r75/r76 (`reports/ac6-retail-native-codegen-gate2-r11-20260831.md`) report
the bounded entry probe timing out after the IB bootstrap batch, with the
main guest thread "parked in `sub_821E6AC8 → sub_821E61A8 → sub_821E64A8`,
queue/ring wait not yet signaled by a real Vd backend." That naming was never
verified against the actual retail US disassembly — it was carried forward
from earlier probe/GDB observation. This cycle establishes, from
`ghidra-projects/ac6-us` only (headless, read-only, `DumpUsWaitFrontier.java`
/ `DumpUsWaitWriters.java` / `FindDirectCallsTo.java` /
`DumpUsFenceCallers.java` in `scripts/`), exactly what these three addresses
do and what they wait on.

A superficially similar address, `0x821E6AC8`, was investigated in cycle 295
(`reports/cycle-295-821e6ac8-closure-and-821edd60-frontier.md`) — but that
was the **PAL** demo/N2 project (`ac6-xbox360-pal`), where the same hex value
turned out to be an internal instruction of `sub_821E6A88`, not a function.
In the **US retail** project used by this thread, `0x821E6AC8` is a real,
independent function entry with different code. The two builds do not share
addresses; cycle 295 is not evidence here and is cited only as the reason
this cycle re-derived everything from `ac6-us` instead of reusing it.

## Established (US retail, `ghidra-projects/ac6-us`, `default.xex`)

- `sub_821E64A8` (`0x821e64a8..0x821e651f`, confirmed function entry, 8
  incoming references) pushes a two-dword packet (`0x5c8`,
  `0x00020000`) into a ring at a write cursor (`object+0x30`, bounds-checked
  against `object+0x38`, flushing via `sub_821E60A8` if exceeded), advances
  the cursor, and conditionally calls `sub_821E61A8(object, 4)` if
  `object+0x2a9c != 0`. It then **unconditionally spins**:
  ```
  821e6500: lwz r11, 0x2af8(r31)
  821e6504: cmpwi cr6, r11, 0x0
  821e6508: bne  cr6, 0x821e6500
  ```
  reading `object+0x2AF8` until it is exactly zero, with **no bounded retry,
  no timeout, no yield call** in this loop. This is the actual stall the
  probe hits, and it can only end if something else, outside this function,
  writes `object+0x2AF8` down to 0.

- `sub_821E61A8` (`0x821e61a8..0x821e627b`, confirmed function entry, 10
  incoming references) is a **different** wait: it computes free ring space
  from `object+0x2a90`/`+0x2a9c`, and if space is short, loops calling
  `sub_821E6AC8(&local)` for backoff and re-checking space
  (`0x821e6240..0x821e6268`); if `sub_821E6AC8` returns 0 (timeout consumed)
  it escalates to `sub_821E6A08`. This wait **does** have a bound, via
  `sub_821E6AC8`'s deadline — but it is not the loop the probe is stuck in.

- `sub_821E6AC8` (`0x821e6ac8..0x821e6b9b`, confirmed function entry, 6
  incoming references) is a bounded backoff/deadline helper: 4-iteration
  spin, then reads an elapsed-time field derived from `object+0x2a88` vs. a
  timebase (`r13+0x100` PCR chain, `bl 0x82390870`) and compares the delta to
  `0x1388` (5000). Under the threshold it returns 1 (keep retrying); at/over
  it calls `sub_821EFAF0` and returns 0 (give up).

- `object+0x2AF8` (the field `sub_821E64A8` spins on) is a **counting
  fence**, adjusted by `sub_821E5FD0` (`add r11,r11,r26`, guarded by a
  lock-acquire/release pair `bl 0x823d045c` / `bl 0x823d047c`). Its four
  static direct-call sites (`FindDirectCallsTo.java 0x821e5fd0`, exhaustive
  over the whole image) are `0x821e5f80`, `0x821e6130` (both inside
  `sub_821E5E48`/`sub_821E60A8`), and `0x821ef2ac`/`0x821ef2f4` (both inside
  `sub_821EF148`). **All four load the delta argument (`r7`) as `0x0` or
  `0x1`** immediately before the call (`DumpUsFenceCallers.java`) — every
  statically-found direct call only increments or no-ops the fence; **none
  decrements it**.

- The only unconditional write of `0x2AF8` to a literal value is
  `sub_821EFAF0` (`stw r10,0x2af8(r31)` with `r10=0`, at `0x821efb64`), the
  timeout-abort path reached only from **`sub_821E6AC8`'s** deadline (the
  `sub_821E61A8` wait, not `sub_821E64A8`'s). Depending on a recoverability
  flag (`object+0x3468`), that path either force-clears the fence and logs,
  or executes `twi r0,0x16` (a hard PPC trap/assert). It is unreachable from
  `sub_821E64A8`'s own spin, which calls nothing.

## Not established

- **Where the real decrement happens.** No direct `bl` to `sub_821E5FD0`
  with a negative or otherwise-decrementing delta exists anywhere in the
  image. The completion signal is therefore either an **indirect call**
  (a registered graphics/CP interrupt notification callback — consistent
  with STATE.md's r53 note that "le consommateur PM4/Vd natif n'est pas
  encore relié") or a different function entirely that touches the same
  field through a different code path not yet located. `FindDirectCallsTo`
  only finds direct `bl`; it does not see `bctrl` dispatch through a
  function-pointer table, which is exactly the mechanism the native Vd
  service's interrupt path (`native/src/native_guest_vd.cpp`) would need to
  drive once IB processing genuinely completes.
- Whether `sub_821EF148` (containing two of the four increment sites) is
  itself the enqueue side of the exact packet `sub_821E64A8` pushes, or an
  unrelated user of the same generic fence primitive.
- Whether `object+0x2AF8` is unique per submission or a single running
  counter across the whole ring (the `object+0x2a9c != 0` guard on whether
  `sub_821E64A8` even calls `sub_821E61A8` suggests per-instance state, not
  yet traced).

## Decision

Do not implement a decrement in `materialize_native_import_stubs.py` /
`native_guest_vd.cpp` this cycle. The only candidate write path found is
indirect and unlocated; writing a decrement now, without knowing what
retail event is supposed to trigger it, would be exactly the synthetic-state
shortcut `CLAUDE.md` and r53 rule out (the ring readback case). The next
cycle's static work is to find the indirect dispatch: search for a
function-pointer field written near the CP/graphics interrupt setup
(`VdSetGraphicsInterruptCallback`-shaped) whose target ends up calling
`sub_821E5FD0` indirectly, or that otherwise reaches `object+0x2AF8`, using
an indirect-branch/vtable-style scan (`FindVirtualDispatchSlot.java`,
`FindPpcBranchesTo.java`) rather than another direct-call search.

No native code, generated codegen, or test was changed in this cycle; CTest
remains **9/9** (verified, unaffected — no C++/Python source touched).
`tools/audit_ac6_mission01_native_gate.py` fails on
`reconstruction/ace-combat-6/src/retail_session.cpp` (`evidence size
mismatch`); this is a pre-existing condition in the abandoned N2 thread's
already-modified working tree (present before this cycle started, per the
Gate0 decision this thread is superseded — `reports/ac6-retail-native-gate0-20260830.md`)
and unrelated to any file this cycle touches.

New read-only tooling: `scripts/DumpUsWaitFrontier.java`,
`scripts/DumpUsWaitWriters.java`, `scripts/DumpUsFenceCallers.java`.
