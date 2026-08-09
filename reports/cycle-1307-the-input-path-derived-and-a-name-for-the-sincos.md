# Cycle 1307 — the input path, derived, and a name for the sincos

## Qualification

- Ghidra projects `ghidra-projects/ace-combat-6` (input path) and
  `ghidra-projects-xenon/ac6-xenon` (the sincos control).
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## Thread A restarts on scalar ground

Cycle 1306 stopped the instrument thread and said Thread A should begin where the
vector layer is not needed. This is that: the retail input path, integer and
scalar from the kernel boundary to the point where it meets the flight maths.

The starting claims were **inherited, not derived**. `CURRENT_PLAN.md` is
bannered superseded, and its "canonical `device+0x3E`", "raw `device+0x4E`" and
"`0x8234D110` splits to `+0x28`/`+0x2A`" came from bridge-era cycles whose bridge
is no longer in the workspace. All three turn out to be right. None of them was
evidence until now.

## From the kernel boundary down

`analysis/address_catalog.tsv` names `0x823911C0` as the `XamInputGetState`
wrapper with provenance **cross-match** — the recompiled side. Read statically it
is three instructions:

```
823911c0 or r5,r4,r4 ; li r4,0x1 ; b 0x823d737c
```

so `XamInputGetState(userIndex, 1, pState)`, tail-calling the import stub
`0x823D737C`. A scan of 827,798 instructions finds **exactly two callers**:
`0x8234D3F0` at `8234d414` and `0x8234D478` at `8234d4d8`.

`0x8234D510` is the poll entry. It dispatches on `[this+0x08]` — the connection
state — to the connected reader or the reconnect reader, stores the result back
to `+0x08`, and copies `[this+0x44]` to `+0x54`.

**`XINPUT_STATE` lives at `device+0x44`**, from `addi r4,r31,0x44` at
`8234d40c`. Two independent confirmations: `+0x44` is the packet number the poll
entry preserves, and `8234d38c lhz r11,0x48(r31)` reads `wButtons`, which the
structure places at base + 4. So the fields are

| device | XINPUT_STATE | field |
|---|---|---|
| `+0x44` | `+0x00` | `dwPacketNumber` |
| `+0x48` | `+0x04` | `wButtons` |
| `+0x4A`/`+0x4B` | `+0x06`/`+0x07` | triggers |
| `+0x4C`/`+0x4E` | `+0x08`/`+0x0A` | `sThumbLX` / `sThumbLY` |
| `+0x50`/`+0x52` | `+0x0C`/`+0x0E` | `sThumbRX` / `sThumbRY` |

## The axis stage, and the table that drives it

`0x8234D110` copies the four thumb axes verbatim into a second block —
`+0x4C→+0x3C`, `+0x4E→+0x3E`, `+0x50→+0x40`, `+0x52→+0x42` — so **`device+0x3E`
is `sThumbLY`**, which is the inherited claim, now read.

It then runs four iterations over a table at **`0x8201250C`**, stride `0xC`,
each entry three halfword indices addressed as `(index + 0x14) * 2`:

| entry | source | positive | negative | axis |
|---|---|---|---|---|
| 0 | `+0x3C` | `+0x2E` | `+0x2C` | LX |
| 1 | `+0x3E` | `+0x28` | `+0x2A` | **LY** |
| 2 | `+0x40` | `+0x36` | `+0x34` | RX |
| 3 | `+0x42` | `+0x30` | `+0x32` | RY |

with the rule, from `8234d154`–`8234d1a0`:

```
v = (s16) raw
if (v >= 0)   pos = raw,  neg = 0
else          pos = 0,    neg = -1 - v
```

`-1 - v` maps `-32768…-1` onto `32767…0`, so both halves are non-negative and
symmetric. Entry 1 is the inherited `+0x28`/`+0x2A` pair for LY, and the table
says which is which rather than leaving it to be guessed.

## The button stage

`0x8234D378`, thirty instructions, is a complete edge model:

| device | value |
|---|---|
| `+0x1C` | current mask, `wButtons` zero-extended |
| `+0x74` | previous mask, written last |
| `+0x14` | **pressed** = `(prev ^ cur) & cur` |
| `+0x18` | **released** = `(prev ^ cur) & ~cur` |
| `+0x20` | `~cur` |

and it calls `0x8234D210(this, cur != prev)`, the inequality computed as
`cntlzw(cur - prev) >> 5 ^ 1` rather than a branch.

That is portable, scalar, and citable — the first gameplay behaviour this
session has that could carry a contract entry.

## A name for `0x8209CB70`, and the control it suggested

It was proposed that `0x8209CB70` is `XMScalarSinCos`, on the signature
`void(float* pSin, float* pCos, float value)` and the shape of the instruction
mix. The identification fits and **corroborates a measurement rather than
replacing one**: cycle 1302 already measured `out[r3] = sin`, `out[r4] = cos`
exactly, and `XMScalarSinCos` puts `pSin` first — two independent routes to the
same argument order.

One correction to the proposal: `cos` is at frame `+0x54`, not `+0x58`. `+0x58`
receives a constant loaded from `0x820542B8`, measured as `0.0`. The row material
is therefore `sin` at `+0x50`, `cos` at `+0x54`, zero at `+0x58`, `-sin` at
`+0x5C`.

The angle list the proposal implies closed a gap cycle 1302 left open — negative
angles and beyond ±π were untested. Seven more:

| θ | `pSin` | `sin θ` | `pCos` | `cos θ` |
|---:|---|---|---|---|
| −π/2 | −0.9999999 | −1.0 | +0.0000001 | 0.0 |
| −1.0 | −0.8414710 | −0.8414710 | +0.5403023 | +0.5403023 |
| −0.5 | −0.4794255 | −0.4794255 | +0.8775826 | +0.8775826 |
| +π/2 | +0.9999999 | +1.0 | +0.0000001 | 0.0 |
| +π | −0.0000003 | −0.0 | −0.9999997 | −1.0 |
| −π | +0.0000003 | +0.0 | −0.9999997 | −1.0 |
| 2π | −0.0000002 | −0.0 | +1.0000000 | +1.0 |

**Fourteen angles now**, spanning both signs and past ±π, all within the residue
of a minimax approximation. The routine is settled.

It does **not** unblock `0x822A1E80`: that is stuck on `vpermwi128`'s lane order,
which cycle 1306 established needs an oracle. Naming the sincos does not name
the permute.

## Not established

- What `0x8234D210` does with the change flag.
- The trigger stage. `+0x4A`/`+0x4B` are in the structure and no reader of them
  has been found yet.
- Which consumer reads `+0x28`…`+0x36`. That is the join to the flight maths and
  it is the next question.
- Whether `0x8234D1B8`, called between the axis and button stages, matters.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
```

## Next

Find the consumers of `device+0x28`…`+0x36` with `tools/ghidra_scripts/Ac6FieldRead.java`,
which splits a displacement load into field reads, vtable dispatches and stack
slots — cycle 1147 measured a plain `grep` at 63% wrong on this exact question.
That names the join between input and flight, and it is still scalar.
