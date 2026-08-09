# Cycle 1353 — the consumer uses the rule the product implements

## Qualification

- **No oracle pass.** Ghidra was used to read three constants; the rest is the
  recompiled corpus and `.pdata`.
- No product C++ changed, no contract changed.

## Three instructions worth the whole cycle

`0x82211C10` is the per-player input consumer, 121 instructions, `.pdata`
agreeing and `short=0`. Inside its inner loop:

```
addi   r11,r10,3
rlwinm r11,r11,2,0,29        ->  r11 = (bit + 3) * 4
lfsx   f13,r11,r4            ->  read that slot of the input record
```

`float_slot_for_bit(bit) = (bit + 3) * 4` is in
`include/ac6/retail_input_record.h`, ported at cycle 1323, **derived at cycle 1318
from the producer's bit-scan** — `addi r9,r10,0x3` followed by
`rlwinm r9,r9,0x2` at `0x821CAED4`.

Here is the same rule in the **consumer**, a function that shares no code, no
register allocation and no derivation with the producer. The rule was inferred
from one side, ported, contracted, and is now read from the other.

That is the first time in this campaign that a ported rule has been met coming
the other way.

## What the binding layer does

Two nested loops of 32. The outer walks 32 output slots gated by
`[player+0x88]`; each slot has a 32-bit mask at `player+0x08+4*i`, and every set
bit in it **names a record flag bit**. Each binding has a 24-byte descriptor at
`player + 24*(bit + 6)`.

The axis rule, instruction by instruction:

```
f0 = |value| − [binding+0x10]          deadzone subtracted
if f0 < 0        -> f0 = 0.0
else  f0 = f0 * [binding+0x14]         scaled
      if f0 > 1  -> f0 = 1.0           clamped
f13 = fsel(value, f0, −f0)             the input's sign restored
[binding+0x04] = f13
```

and a second output from the same slot is a **three-state sign** — `0.0` below
the deadzone, `±1.0` above `[binding+0x0C]`, via `fsel` against the constants
`1.0` and `−1.0`.

Both are negated when `[player+0x8C]` has the slot's bit, written into the
caller's arrays, and the slot is marked in the mask at `r5`.

**So each player has a per-binding deadzone, scale and threshold**, and the game's
axis handling is table-driven rather than hard-coded — which is a thing a port has
to reproduce as a table, not as constants.

## A deadzone that is not the one already found

`retail_input_record.h` records that the `0x800` deadzone cycle 1315 read does
**not** gate the producer's store — a half of 1 already writes `1/32767`, measured
at cycle 1323 over 255 points.

This is where a deadzone actually applies: **downstream, per binding, from
`[binding+0x10]`**, on the value the record already holds. The two facts fit
together and neither needed changing.

## Constants, read not assumed

`−1.0` at `0x82069B28`, `1.0` at `0x82001348`, `0.0` at `0x8200082C`. The last two
are the same words cycles 1330 and 1342 identified for unrelated reasons — the
image reuses one zero and one one throughout, which is now observed three times.

I also computed one of these addresses wrong mid-cycle and dumped `0x82069AE8`,
getting `2.36e21`. The arithmetic was re-done and the right word is `−1.0`. A
nonsense float is a cheap tell; a plausible one would not have been.

## Not established

- What consumes the four output arrays.
- What `this` is.
- Why the descriptor index starts at `+6`.
- Whether all 32 output slots are ever used.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

This is portable. The binding layer takes a contracted input record and a table
of per-binding descriptors and produces two floats per slot, with every constant
read and every branch enumerated — no arena, no dispatcher, no unnamed class. It
is the first piece of A3.2 that could carry a native behaviour and a differential
in one cycle, which is what the plan asks a slice to end with.
