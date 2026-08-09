# Cycle 1352 — where the input records reach gameplay

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus was read, and the
  listing checked against `.pdata`.
- No product C++ changed, no contract changed.

## The constant is one already under contract

`0x82211DF8` is 45 instructions — `.pdata` declares 45, `short=0`. Two of them:

```
lis  r10,-32145
addi r30,r10,-9320
```

`0x826F0000 − 0x2468 = 0x826EDB98`. That is the **input record array**, the base
`analysis/input-path/input-record-layout.tsv` carries and the
`retail_input_record` behaviour cites in `mission01-playable-gate-v1.json`.

The loop bound is `r30 + 640` with a stride of `160`, so it walks **exactly the
four `0xA0`-byte records**. Cycle 1320 established that base and stride by
micro-executing the *producer*; this is the *consumer*, read independently, and
it agrees on both.

## The loop

```
for i in 0..3:
    r3 = this + 4    + 912 * i
    r4 = 0x826EDB98  + 160 * i        <- the input record
    r6 = this + 0xE58
    r7 = this + 0xED8
    r8 = this + 0xF58
    r9 = this + 0xFD8
    bl 0x82211C10
```

**Record `i` goes to player block `i`**, at a per-player stride of 912 bytes,
with four scratch arrays passed alongside. The whole body is skipped when
`[this+0x08]` is zero, and the entry clears two 128-byte float arrays with a
32-iteration loop that writes `[r11−0x80]` and `[r11]` each pass.

## Which answers something cycle 1320 left open

The producer `0x821CAA50` fills **all four records from driver-pointer slot 0** —
measured under three controller plans, and reported at the time as a fact about
the synthetic service the harness built, with the caveat that it could not speak
for retail's contents.

This is the other end. The consumer hands **record `i` to player block `i`**. So
the four records are per-player by construction on the consuming side, whatever a
given service populates on the producing side. The two statements sit together
without either being weakened.

## A prediction from the plan that did not hold

The plan named this function as *"`0x82211DF8` and the float it receives"*.

**It receives no float.** `0x82211DF8` takes `r3` alone; no `f1` appears in its
45 instructions. The float the plan was pointing at is somewhere else — most
likely in `0x82211C10`, which is called with four scratch arrays and a record and
has not been read.

That is worth stating plainly rather than quietly reinterpreting: a named
expectation from the plan met a measurement and lost.

## Not established

- What `0x82211B40`, `0x82211C10` and `0x82211988` do.
- What `this` is.
- What the four arrays at `this+0xE58`, `+0xED8`, `+0xF58`, `+0xFD8` hold.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

`0x82211C10` — it takes the record, the player block and the four arrays, and it
is where the float the plan expected has to be if it exists. This is also the
first function in A3.2 whose inputs are **already contracted**: the record's
layout, its axis rule and its button map are ported, tested and pinned, so
whatever this function reads out of `r4` can be checked against them rather than
guessed.
