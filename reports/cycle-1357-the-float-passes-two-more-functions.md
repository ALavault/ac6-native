# Cycle 1357 — the float passes two more functions

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus was read.
- No product C++ changed, no contract changed.

## Yesterday's "only consumer" is not a consumer

Cycle 1356 ended: *"`0x82211B40` — it takes the float … It is the only consumer of
that float so far identified."*

`0x82211B40` is 51 instructions and contains **no floating-point instruction at
all**. Neither does `0x82211C10`, in 121. `f1` enters `0x82211DF8`, survives both,
and is consumed by `0x82211988` — the **third** call.

## Which corrects the shape written one cycle ago

The thirtieth shape said the check is *"is it written before the first call"*.
That is too narrow, and this is the case that shows it: a volatile register
survives **every** callee that does not write it, so its destination can be
arbitrarily far down the chain.

The rule now reads: **follow the register forward through the callees, testing
each for a write, until one uses it.** Stopping at the first call finds the wrong
consumer as confidently as reading one body finds none — which is exactly what
happened, one cycle apart, in the same thread.

## What `0x82211B40` actually does

It gathers a mask, and its input is a field already under contract:

```
if [r4 + 0x08] == 0 -> return          the RECORD'S FLAG WORD
for each of 32 mask words at player+0x08:
    if word & [r4 + 0x08] : [r5] |= 1 << its index
```

One bit per player-block mask word that intersects the record's flag word — the
same 32 words `0x82211C10` walks, and the same `record+0x08` that
`retail_input_record` ports.

`.pdata` has **no row** for this function, and
`check_listing_against_pdata` says so rather than guessing, so its length has no
independent control.

## And `0x82211988` computes edges the product already implements

```
r9  = [this + 0xE44]     this frame's active-slot mask
r10 = [this + 0xE48]     last frame's, saved at the top of 0x82211DF8
[this + 0xE4C] = r9 & ~r10        newly active
[this + 0xE50] = r10 & ~r9        newly inactive
```

`button_edges` in `retail_input.h` computes `(prev ^ cur) & cur` and
`(prev ^ cur) & ~cur` on device buttons. `cur & ~prev` and `prev & ~cur` are the
same two sets. **The same edge algebra, one level up, on slots rather than
buttons**, read from a function that shares nothing with the one it was derived
from.

That is the second time in three cycles that a rule ported from the device end
has turned up again at the gameplay end.

## Not established

- What `0x82211988` does with the float — four mentions of `f1` in 109
  instructions, unread.
- Why `+0xE4C` and `+0xE54` receive the same value.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 12 behaviours
ctest                                100% passed, 0 failed out of 31
instrument_discipline_index          pass, 21 shapes, 0 unindexed
tools/tests                          Ran 72 tests, OK
```

## Next

`0x82211988`, properly this time: 109 instructions, four of them touching `f1`,
and it is where the float the whole tick carries finally does something. It is
also the last unread function in `0x82211DF8`.
