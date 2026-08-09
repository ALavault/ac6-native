# Cycle 1315 — the deadzone, and the two reciprocals

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The narrow question, answered

*Does `0x821CAA50` write the four `0xA0` records, and from what?*

**It writes two fields per record and no more.** Stores relative to `r23`, which
`821caac8` materialises as `0x826EDB98`:

```
821cb34c stb r10,0x94(r23)     821cb364 stw r9,0x98(r23)
821cb36c stb r10,0x134(r23)    821cb384 stw r9,0x138(r23)
821cb38c stb r10,0x1d4(r23)    821cb3a4 stw r9,0x1d8(r23)
821cb3ac stb r10,0x274(r23)    821cb3c8 stw r11,0x278(r23)
```

Four pairs, stride `0xA0`, which is the record array cycle 1313 found. The
records start at `r23+0x08`, so within a record these are **`+0x8C` (byte) and
`+0x90` (word)**.

**And that matters more than it looks.** The frame stage clears `0x84` bytes per
record — `+0x00…+0x83`. These two fields sit **outside** the cleared range, so
they are state that survives the frame boundary while everything before them does
not. Whatever the rest of the record holds, it is rebuilt every frame and these
two are not.

Where the rest is written is **not established**: `0x826EDBA0` is materialised
nowhere, so no function reaches a record by its own address, and the remaining
writers must arrive through a pointer this cycle has not traced.

## The consumer's shape

```
821caa60 stwu r1,-0x2f0(r1)          ; a 752-byte frame
821caa68 memset(r1+0x60,  0, 0x40)
821caa78 memset(r1+0x140, 0, 0x100)  ; the snapshot buffer, four times the 0x40 read
821caa88 li  r20,0x0                 ; the controller index
821caa9c bl  0x82337e70              ; (1, index)
821caaa4 bl  0x82337e28              ; the snapshot read, into r1+0x140
...
821caab0 cmplwi cr6,r31,0x4          ; four of them
```

So it polls each of the four pads into a stack buffer through the API cycles
1309–1310 derived, then converts.

## The axis conversion, and the deadzone

```
821cb244 subi   r9,r9,0x4000     ; bias
821cb248 extsh  r7,r9
821cb24c cmpwi  cr6,r7,0x800     ; the threshold
821cb250 ble    cr6,0x821cb290   ; below it, the lane is skipped entirely
...
821cb278 fcfid  f0,f0            ; integer to double
821cb284 frsp   f0,f0            ; to float
821cb288 fmuls  f0,f0,f30        ; scale
821cb28c stfsx  f0,r8,r11        ; into an indexed float slot
```

A **bias of `0x4000` and a deadzone of `0x800`** — 2048 of 32767, about 6.25% —
and below it nothing is written at all, so the slot keeps whatever the caller
left there.

**The two scale constants are exact reciprocals**, which is what makes this a
derivation rather than a description:

| register | address | bits | value |
|---|---|---|---|
| `f30` | `0x82069DF0` | `38800200` | `float32(1 / 16383)` |
| `f31` | `0x82069BFC` | `38000100` | `float32(1 / 32767)` |
| `f29` | `0x820542BC` | `3f800000` | `1.0` |

`16383` is `0x4000 − 1` and `32767` is `0x8000 − 1`. The second is exactly the
range of the axis halves cycle 1307 derived — `split_axis` maps a negative raw
value through `-1 - v` onto `0…32767` — so `f31` normalises a half to `[0, 1]`
with the endpoint included, and `f30` does the same for a value the code has
already biased by `0x4000`.

Neither was fitted. Both were read as words, and the identification is a bit
pattern matching `float32(1/n)` for exactly one `n`.

`0x820542B8`, the neighbour of `f29`, is the `0.0` the sincos path stores at
frame `+0x58` — the same constant table serves both.

## The record array's consumers are bounded

`0x826EDB98` is materialised at **nine sites**: the frame stage `0x821CA918`,
this consumer `0x821CAAC8`, the edge handler `0x821CB6C8`, two static-init sites,
and **four other readers** — `0x82211E3C`, `0x82229304`, `0x82256234`,
`0x822AA7F4`. That is the input's whole surface for the rest of the game, and it
is four functions.

## Not established

- What the byte at `+0x8C` and the word at `+0x90` mean.
- What writes `+0x00…+0x83` of a record, which is most of it.
- Which float slot each axis lands in. The index comes from a bit-scan loop
  (`rlwinm r9,r9,0x1f,0x1,0x1f` iterated) that turns a mask into an ordinal, and
  the mask is not yet traced to its source.
- What the four other readers do. `0x82211DF8`, the float consumer from cycle
  1313, is not among them.

## Gates

```
mission01_final_gate (playable-v1)  JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28
contract_artifacts=pass cited=35 match_head=35
tools/tests: Ran 72 tests, OK
```

## Next

The bit-scan loop and the mask it consumes: that is what decides which float slot
an axis reaches, and it is the last thing between the derived snapshot and a
named per-axis output. It is scalar, so the micro-execution instrument can
certify it — which means `0x821CAA50` can carry a contract entry the way
`retail_input` does.
