# Cycle 1400 — a caller that only commands level

## Qualification

- **No Ghidra run and no oracle pass.** The corpus and the image.
- No product C++ changed; ctest stays 47. **No contract entry.**
- `analysis/flight/command-caller-search.tsv` extended.

## The first of the eight, read

`0x82290E20` is a **genuine caller** of the command setters. Its five dispatch
sites carry the signature exactly:

```
lwz    r10,0(r29)        the object's vtable
fmr    f1,f31            the INCREMENT
mr     r3,r29            this
lfs    f2,2092(r11)      the TARGET
lwz    r11,48(r10)       slot 12   (or 56, slot 14)
bctrl
clrlwi r11,r3,24         the return code, tested
```

## Which confirms cycle 1393 from the other end

That cycle derived, by reading the setters, that `f1` is the increment added to
the accumulator and `f2` is the target angle. This is a caller that shares no
code with them, and it agrees: `f1` computed, `f2` a constant, the return code
tested as a boolean.

**Third mapping in this campaign met from both ends**, after `float_slot_for_bit`
at cycle 1353 and the accumulator crossing at 1393. That is the strongest form of
confirmation available without an oracle, and it is now a pattern rather than a
coincidence: derive from one side, meet it coming the other way.

## But it is not the input path

**Every one of its five sites passes `f2 = 0.0`** — the image's zero at
`0x8200082C`, held in `f30`.

So this function only ever commands **level**. It is a recovery or auto-level
behaviour, not stick steering, and on a changed return it increments a counter at
`[r30+744]` — the shape of "how many axes did I have to correct".

Its constants fit: **π/3** at `0x82069C28`, plus 0.95, 0.5 and 500.0, all
thresholds.

And π/3 **is** `float32(π/3)`, checked rather than assumed. This subsystem has
produced four constants that look like reciprocals of π and are seven-digit
decimal literals — `0.3183099` twice, `0.15915495`, `0.6366198` — so the check is
now reflexive. This one is genuine.

## What it leaves

The demo's invented link is **unchanged**: the caller that turns a stick into a
command is still not found. Seven of the eight candidates remain, and the
population is small enough to exhaust.

What did change is that the setters' argument reading is no longer a
single-sided derivation, which matters for the port whether or not the input
caller is ever found.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 4 (1395, 1396, 1399, 1400) |
| implementation/integration spent on A3.3 | 2 (1397, 1398) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 47
tools/tests                           Ran 77 tests, OK
```

## Next

`0x822911E8` — 538 instructions, three sites at offset 56, **four callers**, and
it sits in the same `0x8229xxxx` span as the one just read. If it also commands
zero, that span is the auto-level subsystem and the input path is elsewhere;
if it passes something computed, it is the better candidate.

The discriminator is one line either way: **what `f2` is at each site.**
