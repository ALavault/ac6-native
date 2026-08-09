# Cycle 1359 — the thirteenth behaviour

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass.** `0x82211988` was micro-executed nine times.
- **Product C++ changed**, and the **thirteenth behaviour** is under contract.

## The port

`retail_slot_repeat` is the slot edge and auto-repeat bank. Three edge words and
thirty-two `(timer, limit)` pairs, with the initial delay and the repeat interval
read from fields rather than baked in.

Three details the tests pin, each a place a plausible port would differ:

- **equality fires.** `fcmpu` then `blt` skips the fire, so the slot fires when
  the timer is *not less* than the limit. A port written with `>` holds one frame
  longer, every repeat, forever.
- **the reset happens before the add.** A slot that has just fired holds
  `elapsed`, not zero — the add is on both paths, after any reset.
- **releasing restores the initial delay**, not the repeat interval, so a slot
  tapped twice waits the full delay again.

## A differential that matters more than usual

`.pdata` has **no row** for `0x82211988`, so its length has no independent
control and the port rests on the recompiled corpus alone. The differential is
the only check that does not depend on the listing being complete.

Nine cases, all three edge words and **all 32 pairs** compared bit for bit,
including the timer exactly at the limit, just under it, a slot released, a slot
far past its limit, slot 31, and all 32 held at once. **Nine of nine, first run.**

## The edges agree with a rule the product already had

`button_edges` computes `(prev ^ cur) & cur` and `(prev ^ cur) & ~cur` on device
buttons. This computes `cur & ~prev` and `prev & ~cur` on slots. The test asserts
the two agree rather than describing them as similar — if they ever diverge, one
of the two derivations was wrong.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 13 behaviours
ctest                                100% passed, 0 failed out of 32
contract_addresses                   pass
contract_derivations                 pass
slot_repeat_microexec                pass, 9 cases, 0 divergences
tools/tests                          Ran 72 tests, OK
```

## Where the input path stands

`0x821CA908`, the frame tick, has six pieces. **Four are now behaviours in the
gate** — the record producer, the binding layer, the record itself and this — and
the remaining two are read and recorded: the virtual that yields the elapsed time,
and the mask gatherer `0x82211B40`.

## Not established

- What consumes the three edge words.
- Where the flight integrator's time comes from. Cycle 1358 established it is not
  this float.

## Next

`0x82211B40` is the last unported piece of the tick: 51 instructions, no
floating-point, and its input is `record+0x08`, which is already contracted. It
would make the tick complete in the product.
