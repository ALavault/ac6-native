# Cycle 1316 — a flag word, and a parallel float array

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The bit-scan, read

The loop cycle 1315 could not place is a **compiled `log2`**:

```
821cacb0 rlwinm r10,r10,0x1f,0x1,0x1f   ; r10 >>= 1
821cacb4 addi   r9,r9,0x1
821cacb8 cmplwi cr6,r10,0x1
821cacbc bne    cr6,0x821cacb0          ; until the mask is 1
```

Twenty sites, each shifting a **constant** mask down to `1` and counting. The
compiler did not fold them, which is what makes them readable: the constant is
the flag's bit, and the count is its position.

The masks are `lis r10,0x1` = `0x10000`, `lis r10,0x2` = `0x20000`,
`li r10,0x4000`, and `r21 = 0x8000` — bits 16, 17, 14 and 15.

Then the position becomes an offset:

```
821caed4 addi   r9,r10,0x3
821caedc rlwinm r7,r9,0x2,0x0,0x1d     ; (position + 3) * 4
821caea8 stfsx  f0,r10,r11             ; store at r11 + that
```

## And `r11` is the record

```
821cab64 lwz r11,0x6c(r1)
821cab70 add r11,r22,r11
821cab78 add r11,r11,r23      ; r23 = 0x826EDB98, the record array
```

So the floats land **inside the record**, at `record + (bit + 3) * 4`.

## Which makes the whole record legible

Just before, thirteen `or`/`stw` pairs accumulate a mask one bit at a time and
store it at the same place each round:

```
821cabe0 or  r9,r10,r8
821cabe4 stw r10,0x8(r11)
821cabe8 or  r10,r9,r7
821cabec stw r9,0x8(r11)
...  thirteen of them, r25..r31 and r3..r10
```

So the record is:

| offset | what |
|---|---|
| `+0x08` | a **flag word**, accumulated from thirteen sources |
| `+0x0C … +0x50` | a **parallel float array**, slot = `(bit + 3) * 4` |
| `+0x8C`, `+0x90` | the two fields cycle 1315 found, written once per pass |

Bit 0 lands at `+0x0C` and bit 17 at `+0x50`, so the array spans `+0x0C…+0x53`.

**That explains the clear.** The frame stage zeroes `+0x00…+0x83` per record and
not the full `0xA0` — the flag word and the float array are rebuilt every frame,
and `+0x8C`/`+0x90` are deliberately outside. Cycle 1315 noticed the boundary was
odd; it is not odd, it is exactly the rebuilt region.

Nothing here was fitted. The clear width came from `0x84` in a loop bound, the
array extent from `(bit + 3) * 4` over the observed bits, and they agree.

## The join to `retail_input`

The analogue sources are stack halfwords read from a `0x40` buffer at `r1+0x60`:

| read | buffer offset | snapshot field | device |
|---|---|---|---|
| `lhz r9,0x86(r1)` | `+0x26` | LY negative half | `+0x2A` |
| `lhz r9,0x88(r1)` | `+0x28` | LX negative half | `+0x2C` |
| `lhz r9,0x8a(r1)` | `+0x2A` | LX positive half | `+0x2E` |

Those are the offsets cycle 1307 derived from the table at `0x8201250C`, and the
scale applied to them is `f31 = float32(1/32767)` — the exact reciprocal of the
range `split_axis` produces. **The consumer reads the layout the port already
implements, and normalises it by the constant that layout implies.** Two
derivations from opposite ends meeting.

## Not established

- What each bit of the flag word means. Thirteen sources were counted, not read.
- Which axis owns which of bits 14–17. The reads are placed, the assignment is
  not, and guessing "LX is 14" from the order they appear would be a rule with no
  control.
- What `+0x8C` and `+0x90` hold. Unchanged from cycle 1315.
- Whether `r20` is zero at every slot computation. It is `li r20,0x0` at the top
  and reused as a zero source, but it is a loop-visible register and no control
  yet says it never changes. Every offset above assumes it is zero.

## Gates

```
mission01_final_gate (playable-v1)  JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28
contract_artifacts=pass cited=35 match_head=35
tools/tests: Ran 72 tests, OK
```

## Next

Settle `r20` first — every offset in this report depends on it and the assumption
is currently unchecked, which is the shape this campaign keeps paying for. Then
micro-execute `0x821CAA50` on a seeded snapshot: it is scalar, the record array
can be a poison region, and the flag word and float array come out of one run.
That is the differential that would let it carry a contract entry beside
`retail_input`.
