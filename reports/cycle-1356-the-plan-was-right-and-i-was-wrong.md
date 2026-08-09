# Cycle 1356 — the plan was right, and I was wrong

## Qualification

- Ghidra was used to list call sites. **No oracle pass.**
- No product C++ changed, no contract changed.

## The whole input tick, in one function

All five calls to `0x82211DF8` are inside `0x821CA908` — the frame input stage
the ladder places on `0x821D7A90`. Read whole, it is the input path end to end:

```
bl 0x821CB5F0                          -> the record producer's caller
r11 = [0x823F6DB8] ; call [r11+0x1C]   -> a virtual, and f31 = its result
for k in (17804, 22256, 26708, 31160):
    r3 = [0x826E4EB4] + 0x20000 + k
    f1 = f31 ; bl 0x82211DF8
r3 = r30 + 0x20 ; f1 = f31 ; bl 0x82211DF8
```

## Two predictions the plan made, and both hold

The plan wrote: *"`0x82211DF8` et le flottant qu'il reçoit ; le virtuel `+0x1C` de
`[0x823F6DB8]` qui produit ce flottant."*

`lis −32193, addi 28088` is **`0x823F6DB8`** exactly. The call is `lwz r11,28(r11)`
— **slot `+0x1C`** exactly. Its return value goes into `f31` and then into `f1`
for every one of the five calls.

And: *"la disposition à `context+0x2458C` (quatre objets, pas `0x1164`, dans le
contexte `[0x826E4EB4]`)."*

The four constants are `context + 0x20000 + {17804, 22256, 26708, 31160}`. The
first is **`context+0x2458C`**. The three gaps are 4452, 4452, 4452 — **`0x1164`**.

Both were written before any of this thread existed, and both are confirmed by
arithmetic rather than by resemblance.

## And a correction to myself, which is the thirtieth shape

Cycle 1352 read all 45 instructions of `0x82211DF8`, found no `f1`, and wrote:
*"It receives no float."* It even offered that as a plan prediction meeting a
measurement and losing.

**It receives `f1`.** The caller sets it before every call, and `0x82211DF8`
forwards it untouched into `bl 0x82211B40` — its first call, reached before
anything could clobber a volatile register.

A pass-through argument is **invisible in the mnemonics**. Nothing mentions it
because nothing has to. Searching a body for a register finds every use and no
forward, and the check is not "is it mentioned" but "is it written before the
first call".

Written down as *the argument that passes straight through*, the sibling of
*reachability by `bl`*: there a function looked unreferenced because the reference
was elsewhere; here an argument looked absent because its use was one frame
further down.

## What is not named

**The float.** It is the return value of a virtual call whose class has not been
read. It is not called delta time, frame time, or anything else — cycle 1299 paid
four cycles for a premise of exactly that kind, and eleven cycles of this thread
have declined to name it.

## Not established

- What class `[0x823F6DB8]` points to, and what its `+0x1C` computes.
- What the four `0x1164`-byte objects are.
- What `0x82211B40` does with the float.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 12 behaviours
ctest                                100% passed, 0 failed out of 31
instrument_discipline_index          pass, 21 shapes, 0 unindexed
tools/tests                          Ran 72 tests, OK
```

## Next

`0x82211B40` — it takes the float, the record array base and the mask word, and
runs once per object before the per-player loop. It is the only consumer of that
float so far identified, and it is bounded: three arguments, all of them already
named.
