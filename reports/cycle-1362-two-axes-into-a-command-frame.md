# Cycle 1362 — two axes into a command frame

## Qualification

- Ghidra was used to list callers. **No oracle pass.**
- No product C++ changed, no contract changed.

## The gap named yesterday is filled

Cycle 1361 noticed what the three edge consumers **do not** touch: none reads the
analogue value arrays `retail_input_binding` fills. It called that the one thread
still pointing at a flight model.

`0x82229250` is the reader. 367 instructions, `.pdata` agreeing, one caller.

```
sub_82229250(r3 = destination, r4 = the input command object)

    [dst+0x838] = [src+0xE58 + 0]     analogue slot 0
    [dst+0x83C] = [src+0xE58 + 4]     analogue slot 1
    [dst+0x84C] = slot 9 of +0xE50    a RELEASED edge
    [dst+0x854] = slot 7 of +0xE4C
    [dst+0x858] = slot 8 of +0xE4C
```

and it **clears those fields at entry** before filling them, so a tick with no
input produces zeros rather than last tick's values — the opposite of the binding
layer's own skip, which preserves.

**Two analogue axes and a handful of discrete bits, assembled per tick into one
structure.** That is the first use of the value array found anywhere.

## What is deliberately not said

The two axes are **not** called pitch and roll. They are slots 0 and 1 of a
table-driven binding layer, and nothing read says which physical control feeds
them — the masks that decide are per-player data this campaign has not seen
populated.

Cycle 1299 paid four cycles for a premise of exactly that kind, and this thread
has declined to name the float, the children and the arena on the same grounds.

## Where it sits

One caller, `0x8223408C`, inside `sub_82234040` — 742 instructions. It is **not**
a vtable slot: `FindDataPointersTo` returns a single hit and it is a `.pdata` row,
which the tool labels rather than leaving to be mistaken for a dispatch slot.

## Not established

- What the destination structure is.
- What `sub_82234040` does with it.
- What fills `dst+0x840`, which is cleared and not written on the path read.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

`sub_82234040`, the one caller. It is 742 instructions and it owns the
destination — so it is where a command frame either becomes a flight input or
does not, and that is the question A3.2 has been circling for fifteen cycles.
