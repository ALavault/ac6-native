# Correcting the gate-3 framing: forcing the readiness flag changes
# nothing, because the pool-poisoned byte was already nonzero

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run (`probe --until frontend
--max-ticks 8000`, fresh neutral store, the forced-`menu_endMode=1` route
plus the new falsifier `AC6_DEMO_FORCE_LOADING_READY_FLAG=1`,
`AC6_DEMO_WATCH_TASK_LIST=1 AC6_DEMO_WATCH_MODE_STATE=1
AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS=1 AC6_DEMO_WATCH_SWG_NATIVE_CALL=1
AC6_DEMO_WATCH_SWG_LISTENER_ARRAY=1`). Source change:
`recompilation/ace-combat-6-demo/src/guest_bridge/frontend_state_trace.hpp`
(new `apply_loading_ready_flag_override`), one call site added in
`guest_bridge/lifecycle.hpp`.

## The falsifier `99ec1791` named, run

Forces the byte at `[[0x827435F8]+0x222BFE]` (`sub_8217E258`'s gate-3
readiness check, `77cfddb5`) to `1` every tick from the moment the manager
pointer resolves onward, mirroring `883d396d`'s `SendMsgI`-result-forcing
falsifier pattern exactly (opt-in env var, write-only, logs only on an
actual value change).

## Result: no effect on anything measured

Every signal this campaign has been tracking on the forced-`menu_endMode=1`
route is **byte-identical** to the unforced baseline (`994109dc`/
`77cfddb5`):

```
milestones = {frontend: false, mission: false, terminal: false}   (same)
task list:   last touched tick 491, never again                   (same)
render-queue payload selector 0x82386DD0: nonzero=1 count = 0      (same)
listener array: {CSelectMessageDlgManager, CModeTaskLoadingDemoOffline}
                 same two objects, same vtables, no new entries    (same)
mode state:  2429 -> 2452 -> 4252 -> 4255 -> 4259 -> settles at
             state=1 tick 5414, never changes again through 8000   (same)
```

The force itself fired correctly and exactly once (`AC6_LOADING_READY_
FLAG_FORCED tick=4259 manager=0x18980000 address=0x18BA2BFE value=1`) --
the write reached guest memory, at the address this campaign computed, at
the tick the loading task first exists to read it. **It changed nothing
downstream.**

## Why: the byte was never actually zero

`99ec1791` confirmed the byte is written once, at tick 4, with the generic
allocator pool-poison fill `0xFEFEFEFE` -- and never again on the
unmodified route. `0xFE` (254) **is itself nonzero**. `sub_8217E258`'s
gate 3 is `flag != 0`, not `flag == 1` or any specific sentinel
(`77cfddb5`'s own transcription of the read: `*out = (flag != 0) ? 1 : 0`).
**The un-forced byte already satisfies this check, every tick, by
accident of never having been cleared.** Forcing it to `1` changes its
exact bit pattern but not its truth value under `!= 0` -- both `0xFE` and
`1` are "nonzero." This falsifier could never have shown an effect from
gate 3 specifically, because gate 3 was never the failing condition in
the first place.

**This corrects `77cfddb5`'s and `99ec1791`'s framing**, not their
measurements: both reports correctly observed the byte is allocated once
and never written again; both incorrectly implied (without stating it as
a distinct, checked claim) that this made gate 3 the operative blocker.
It does not. Gate 3 has been passing, unnoticed, since the moment
`CModeTaskLoadingDemoOffline` was constructed.

## What this narrows the question to

`sub_8217E258`'s two other gates -- `[this-92] == 1` (`[primary_object+12]
== 1`, i.e. `[0x2E3C0200+12] == 1`) and `[this+32] == 0`
(`[0x2E3C0268+32] == 0`, i.e. `[0x2E3C0200+136] == 0`) -- are the ones
this report has not yet measured live. One of them must be failing every
tick, since the handler's overall answer stays `0` throughout the run
(consistent with nothing downstream ever changing) despite gate 3 passing.
This is now a smaller, two-field question, and both fields live on the
same already-located, already-RTTI-confirmed object this campaign has
been reading the whole cycle -- no new object to find, only two words to
watch.

## Not established

- The live values of `[0x2E3C0200+12]` and `[0x2E3C0268+32]` across the
  run -- not watched this cycle (the falsifier run watched the flag byte's
  force, not these two fields). The direct next step.
- Whether gate 1 or gate 2 (or both) is the actual failing condition --
  not distinguished; watching both in one more run settles it.
- Whether the handler's overall accumulator (`[r1+84]` in the marshaller,
  `77cfddb5`) genuinely stays `0` for the whole run, as opposed to
  occasionally succeeding and being overwritten by a later listener in
  the same tick -- the array holds only two listeners and the other is a
  confirmed dead stub, so this is very likely moot, but not directly
  re-checked this cycle.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean before
this commit (below). `apply_loading_ready_flag_override` is opt-in only
(off unless `AC6_DEMO_FORCE_LOADING_READY_FLAG` is set), write-only, same
shape as `apply_swg_sendmsgi_override`/`apply_menu_endmode_arg_override` --
default route behavior is unchanged.
