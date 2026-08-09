# Cycle 1318 — `r20` is zero, and the axis map closes

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The assumption, settled

Cycle 1316 published a record layout resting on `r20 = 0` at every slot
computation and said so. Over the whole 744-instruction function, `r20` appears
**forty times and is written once**:

```
821caa88 li  r20,0x0     <- the only write
821caabc stb r20,0x18(r18)   store OF r20
821cab8c stw r20,0x4(r11)    store OF r20
```

A store names its source first, so those two are reads. And `r20` is
non-volatile in the PowerPC ABI, so the two API callees the function makes must
preserve it. **Zero throughout**, and the offsets `(bit + 3) * 4` stand with no
base term.

That is worth the cycle on its own: an unchecked assumption under a published
layout is the shape this campaign keeps paying for, and it cost one `grep` to
close.

## And the axis map closes with it

Pairing each halfword read from the snapshot buffer with the mask that follows
it gives the whole analogue path:

| buffer | device | half | mask | bit | slot |
|---|---|---|---:|---:|---|
| `+0x24` | `+0x28` | LY positive | `0x20000` | 17 | `record+0x50` |
| `+0x26` | `+0x2A` | LY negative | `0x20000` | 17 | `record+0x50` |
| `+0x28` | `+0x2C` | LX negative | `0x10000` | 16 | `record+0x4C` |
| `+0x2A` | `+0x2E` | LX positive | `0x10000` | 16 | `record+0x4C` |
| `+0x2C` | `+0x30` | RY positive | `0x80000` | 19 | `record+0x58` |
| `+0x2E` | `+0x32` | RY negative | `0x80000` | 19 | `record+0x58` |
| `+0x30` | `+0x34` | RX negative | `0x40000` | 18 | `record+0x54` |
| `+0x32` | `+0x36` | RX positive | `0x40000` | 18 | `record+0x54` |

**Each axis owns one slot, shared by its two halves.** The positive half is
scaled straight; the negative half goes through `821cb044 neg r9,r9` first. So
the eight non-negative halves the device produces become **four signed floats**,
one per stick axis, at `record+0x4C`, `+0x50`, `+0x54`, `+0x58`.

The two device offsets sit exactly where cycle 1307 derived them from the table
at `0x8201250C`, and the four bits are consecutive — 16, 17, 18, 19 — which no
step of the derivation arranged.

## A correction to cycle 1316

It put the float array at `+0x0C…+0x53`, from the bits it had seen then (14–17).
Bit 19 reaches `record+0x58`, so the array extends to at least **`+0x5B`**. Still
inside the `0x84` the frame stage clears, which is the constraint that mattered.

Bits 14 and 15 — masks `0x4000` and `0x8000`, slots `+0x44` and `+0x48` — are fed
by something other than these eight halves. The triggers at `device+0x4A`/`+0x4B`
are the obvious guess and are written down as a guess.

## Not established

- What feeds bits 14 and 15.
- The two reads at buffer `+0x34` and `+0x36` — `device+0x38`/`+0x3A` — which
  fall between the axis halves and the raw thumbs and are not in the layout
  cycle 1310 derived.
- What each bit of the flag word at `record+0x08` means.
- The differential. Everything here is read, none of it is executed.

## Gates

```
mission01_final_gate (playable-v1)  JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28
contract_artifacts=pass
tools/tests: Ran 72 tests, OK
```

## Next

The differential for `0x821CAA50`. It is scalar and the record array can be a
poison region, but the spec is more work than `retail_input`'s was: the function
reaches its snapshot through the service singleton at `0x8290DE00`, so the run
needs that singleton, its `DriverContext` at `+0x24`, the five driver pointers at
`context+0x04`, and one `DriverController` carrying the fields — built as regions
rather than stubbed, so the API path under test is the real one. One run then
yields the flag word and all six analogue slots together.
