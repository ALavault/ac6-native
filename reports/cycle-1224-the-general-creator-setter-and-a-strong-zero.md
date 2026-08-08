# Cycle 1224 — the general creator setter, and a zero that is finally worth something

## What cycle 1220 could not find

Cycle 1220 established that `0x821B54C0` sets the mode creator to **table entry
0** and retired the lead cycle 1218 had named. What writes it with anything else
was unknown.

A force scan of `0x821B5000`–`0x821B6000` — 979 instructions already listed, **35
forced** — shows a family of setters, several of them in the unanalysed gap:

```
821b52a8  stw r6,0x10(r3)     in none
821b54d0  stw r11,0x10(r10)   in none      ; the entry-0 setter
821b5854  stw r10,0x10(r11)
821b58b8  stw r11,0x10(r3)
821b5ce8  stw r11,0x10(r10)   in none
821b5e80  stw r11,0x10(r10)
```

## The general one

`0x821B5808`, read here:

```
821b5840  lwz    r11,-0x45f0(r31)     ; the CTaskModeManager
821b5844  lis    r10,-0x7d97
821b5848  rlwinm r9,r30,0x2,0x0,0x1d  ; index * 4       <- r30 is an ARGUMENT
821b584c  addi   r10,r10,0x1adc       ; 0x82691ADC
821b5850  lwzx   r10,r9,r10           ; table[index]
821b5854  stw    r10,0x10(r11)        ; -> manager+0x10
```

**`SetModeCreator(index)`** — the first site found that takes the index rather
than baking it in. And its table base is `0x82691ADC`, one dword past the base
`0x821B54C0` uses, which is exactly the off-by-one that makes cycle 1218's "entry
45 at `0x82691B8C`" and an argument of **44** the same thing:
`0x82691B8C − 0x82691ADC = 0xB0 = 44 × 4`.

`0x821B5CE0` is a third form, four instructions, fixed on `[0x82691AE4]`.

So the question sharpens from *what selects entry 45* to **who calls
`0x821B5808` with 44**.

## And the answer is: nothing in `.text` does

Scanning the whole of `0x82090000`–`0x823D772C` with disassembly forced, for the
**bare text** `821b5808` and `821b54c0` — not just `bl`, any occurrence at all:

```
scanned=852724  already_listed=846087  forced=6637  hits=0
```

**Zero.** Neither address appears as a call, a branch, or an immediate anywhere
in the program's code.

This is the first zero in this session I am willing to lean on, and it is worth
saying why. Cycle 1220's zero was an artefact of the listing. Cycle 1218's
"no writer found" was void, and cycle 1223 found sixty-five instances of what it
had denied. **This one was taken with an instrument that reports its own
denominator, over 852,724 instructions with 6,637 of them disassembled by the
scan itself, and with the family it was looking for demonstrably visible in the
same run.**

So both setters are reached as **vtable slots or data-table entries**, which is
the shape cycle 1213 warned about and cycle 1218 confirmed for the whole
mission chain: `main` reaches a quarter of this program by direct call, and these
two are in the other three quarters.

## The table itself

`0x82691AD8` onward, dumped:

```
82691ad8  821bb5c8 821bb618
82691ae0  821bb5c8 821bb668
82691ae8  821bb6b8 821bb708
82691af0  821bb6b8 821bb758
82691af8  821bb7a8 821bb7f8
```

Dwords of `.text` pointers into `0x821BB5C8`–`0x821BB7F8`, with every other entry
repeating — a `{shared, distinct}` alternation. `0x821BBF98`, cycle 1218's
`new CModeTaskGame`, is in the same neighbourhood. **I am not calling that a
`{create, init}` pair**; the alternation is a shape, and the two columns' roles
were not read.

## Not established, stated plainly

- **What invokes `0x821B5808`.** It is in no code reference; the next step is
  vtable resolution over `.rdata`, which cycle 1218 showed works and which I did
  not run.
- The two columns of the creator table.
- Whether the index argument is ever 44 in practice. The arithmetic says 44
  reaches `0x82691B8C`; nothing read says anyone passes it.
- Cycle 1216's `[0x82871084]` enumeration, still not re-run.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
852,724 instructions, 6,637 forced, 0 hits for either address as bare text
```

No product code changed.
