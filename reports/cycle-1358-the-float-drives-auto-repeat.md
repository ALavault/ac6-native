# Cycle 1358 — the float drives auto-repeat

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus was read.
- No product C++ changed, no contract changed.

## Where the float goes, at last

`0x82211988` is a bank of **32 auto-repeat timers**:

```
for slot in 0..31:
    if active & (1 << slot):
        if *timer >= *limit:
            triggered |= 1 << slot
            *limit = [this+0x105C]        the repeat interval
            *timer = 0.0
        *timer += f1                       <- the float
    else:
        *timer = 0.0
        *limit = [this+0x1058]             the initial delay
```

Hold a slot and it appears once in the newly-active edge, fires again after the
initial delay, then every repeat interval. Release it and both timer and limit
reset. Thirty-two `(timer, limit)` pairs of eight bytes from `this+0x1060`, and
the loop advances 32 bytes per four slots, which is the same eight.

## So the float is an elapsed time — and it is not the flight model's

`f1` is summed into timers compared against a delay and a repeat rate. **For this
consumer it is an elapsed time per frame**, and that is read from its use rather
than taken from its name.

Eleven cycles of this thread declined to call it delta time, and the plan pointed
at it while looking for the **flight controller**. It drives **input
auto-repeat**. The path from `[0x823F6DB8]+0x1C` through `0x821CA908` to
`0x82211988` is the input subsystem's own clock, and the flight integrator is not
on it.

Whether the same virtual's return is used as a physics delta somewhere else is
not established. Nothing here says it is, and nothing says it is not.

## It also answers yesterday's open question

Cycle 1357 asked why `+0xE4C` and `+0xE54` receive the same value at entry. They
start equal and **diverge here**: `+0xE4C` keeps the newly-active edge, `+0xE54`
accumulates every repeat on top of it. One is the edge, the other is the
triggered set.

## The tick, end to end

Twelve cycles of reading, and `0x821CA908` now has no unread callee:

```
0x821CB5F0   -> 0x821CAA50   produce the four records      contracted
[0x823F6DB8]+0x1C            the frame's elapsed time      read
0x82211DF8 x5                per object                    read
  0x82211B40                 active-slot mask from record+0x08   contracted input
  0x82211C10 x4              the binding layer             contracted
  0x82211988                 auto-repeat timers            read
```

Three of the six are behaviours in the gate; the other three are read and
recorded.

## Not established

- What `.pdata` says about `0x82211988` — it has **no row**, so its length has no
  independent control.
- What consumes `+0xE4C`, `+0xE50` and `+0xE54`.
- Where the flight integrator's own time comes from.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 12 behaviours
ctest                                100% passed, 0 failed out of 31
tools/tests                          Ran 72 tests, OK
```

## Next

The auto-repeat bank is portable and its inputs are contracted — the active-slot
mask comes from `record+0x08`, which `retail_input_record` already carries. It is
a thirteenth behaviour of the same shape as the twelfth: a small rule, a
differential the harness can run, and no arena in sight.
