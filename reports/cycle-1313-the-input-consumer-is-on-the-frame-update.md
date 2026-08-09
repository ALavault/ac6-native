# Cycle 1313 — the input consumer is on the frame update

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## Where it sits

`0x821CA908` has **one caller**: `821d7ae8`, inside **`0x821D7A90`** — the frame
update already in `MISSION01_LADDER.md`'s hop table, the one that runs every
frame from `821d7e34`.

So the chain the last six cycles derived is not off to one side. It is on the
tick:

```
0x821D7A90   frame update, every frame
  0x821CA908   82 instructions, the frame-level input stage
    0x821CAA50   744 instructions, the consumer
```

`0x821CAA50` has **zero indirect branches** over its whole `.pdata` extent —
`bctr=0 bctrl=0 blr=0 blrl=0` — so it is a read, not a dispatcher hunt. It has
two callers, `0x821CA908` and `0x821CB5F0`, and neither it nor `0x821CA908`
appears in any vtable.

## What the frame stage does

**Four records, stride `0xA0`.** The prologue clears them:

```
821ca920 subi r31,r11,0x2468       ; r31 = 0x826EDB98
821ca928 addi r10,r31,0x8          ; first record at 0x826EDBA0
821ca934 li   r9,0x20              ; 32 words
821ca93c stw  r8,0x0(r11) ...bdnz  ; 0x80 bytes, plus the word before
821ca950 addi r10,r10,0xa0         ; stride
821ca954 cmpw cr6,r10,r11          ; bound r31+0x288
```

`(0x288 − 0x08) / 0xA0 = 4` — one record per `DriverController`, matching the
four cycle 1308 found embedded in the `DriverContext`.

Then, in order: `0x82337E18` (the API entry whose argument is the constant `7`),
`0x821CAA50`, and a scan back over the same four records comparing each record's
word 0 against `[this+0x1C]`. When a record shares a bit with it — or carries
bits while `[this+0x1C]` is zero — `0x821CB5F0` runs and `[this+0x19]` and
`[this+0x1C]` are cleared. The whole arm is guarded by `[this+0x19] != 0`.

## And then the frame's float reaches four objects

```
821ca9cc lis  r11,-0x7dc1
821ca9d0 addi r3,r11,0x6db8        ; 0x823F6DB8
821ca9d4 lwz  r11,0x0(r3)
821ca9d8 lwz  r11,0x1c(r11)        ; virtual slot +0x1C
821ca9e0 bctrl
821ca9e8 fmr  f31,f1               ; the result is a float, kept
```

then five calls to `0x82211DF8(object, f31)`:

```
821ca9ec lwz   r11,0x4eb4(r31)     ; [0x826E4EB4]
821ca9f0 addis r3,r11,0x2
821ca9f4 addi  r3,r3,0x458c        ; context + 0x2458C
   ... + 0x256F0, + 0x26854, + 0x279B8      stride 0x1164, four of them
821caa38 addi  r3,r30,0x20         ; and one on this + 0x20
```

**`0x826E4EB4` is the global the ladder already names** — `mode = *([0x826E4EB4] + 0x78)`
is what selects the campaign loader. So the frame stage takes a float from a
virtual call and hands it to four objects at stride `0x1164` inside that context,
plus one of its own.

Four records in, four objects out, on the frame update, with a float between
them. That is the shape of a per-frame input-to-consumer step, and every address
in it is read.

## What is deliberately not concluded

The float is **not** called a delta time here, the four `0x1164` objects are
**not** called players, and the `7` is **not** called a controller mask. Each is
the obvious guess and none is established; `0x82211DF8`, the virtual at
`+0x1C` of `[0x823F6DB8]`, and the layout at `context+0x2458C` are all unread.

Cycle 1299 spent four cycles on an unexamined premise about what a routine
*should* produce. The premise here would be cheaper to check than to assume.

## Not established

- What `0x821CAA50` does. 744 instructions, still unread — this cycle bounded it
  rather than reading it, which is why it was worth doing first.
- What `0x82211DF8` does with the float.
- What the record at stride `0xA0` holds. Only its word 0 is touched here.
- What `0x821CB5F0` does, beyond that it also calls `0x821CAA50`.

## Gates

```
mission01_final_gate (v3 and playable-v1)  JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28
contract_artifacts=pass cited=35 match_head=35
contract_addresses=pass cited=144 supported=144 unsupported=0
tools/tests: Ran 72 tests, OK
```

## Next

Read `0x821CAA50` in its own right. It is straight-line by the indirect-branch
count, 744 instructions, and it sits between four cleared records and four
objects that receive a float. The question to answer first is narrow: does it
write those records, and from what — because the answer names the record layout,
and the record layout is what the flight code will be reading.
