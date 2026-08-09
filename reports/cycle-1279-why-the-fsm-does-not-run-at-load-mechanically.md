# Cycle 1279 — why the FSM does not run at load, mechanically

## Qualification

Delegated investigation; **the load-bearing instructions were re-read here** in
`ghidra-projects-xenon/ac6-xenon`. `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## Established — the initial state is chosen on the child count, and it is zero

`0x822980C8`, the **only** caller of the unit family's `SetInitialState`
(`0x82295030`), picks the state from `[unit+0xDC]`:

```
822980ec  lwz   r7,0xdc(r31)      ; the child count
822980f8  cmpwi cr6,r7,0x0
8229814c  ble   cr6,0x82298164    ; zero children -> the NULL arm
82298150  lis   r10,-0x7dd7
82298158  addi  r10,r10,0x7b20    ; children > 0 -> state 0x82297B20
82298164  stw   r11,0x50(r1)      ; the NULL arm: handler = 0
82298168  stw   r9,0x5c(r1)
82298174  bl    0x82295030
```

and the constructor zeroes that field before anything can fill it:

```
822a23a8  stw r9,0xd8(r31)     ; child array   = 0
822a23ac  stw r9,0xdc(r31)     ; child COUNT   = 0
822a23b0  stw r9,0xe0(r31)     ; order list    = 0
822a23b4  stw r9,0xe4(r31)
```

`0x822986B0` — the constructor that stamps vtable `0x82009440`, the one measured
on 228 of Mission 01's 230 objects — calls `0x822A2330` first and reaches
`0x822980C8` some `0x178` instructions later with nothing in between writing
`+0xD8` or `+0xDC`.

**So construction installs the NULL state.** Cycle 1244 proved the placement does
not run at load by census — three `li 0x7d1`, three `li 0x7d4`, and the push
never fired. This is the same fact with a cause: the FSM is installed dead
because no children are attached yet, and the state that would push has not been
selected.

## Established — the state machine, and one correction to my own brief

The unit family has **three** installers on `fsm = unit+0xF0`, taking a 16-byte
by-value descriptor `{handler, this-delta, w2, w3}` in `r4:r5`:

- `0x82295030` **SetInitialState** — stores, calls the new handler with `r4 = -3`,
  does not exit the old. One caller.
- `0x822950A0` **SetState** — old handler with `r4 = -1`, store, new with `-3`.
- `0x822957E0` the CFsm constructor, writing vtable `0x82008D34`.

with the convention read rather than inferred: `-1` leave, `-2` tick, `-3` enter.

**`0x82297540` is never the initial state.** It is entered from `0x82297B20`, or
from the event slots. And **one of the six materialisation sites does not install
at all** — my brief called them six installs; site 5 copies the *current*
descriptor and compares:

```
822983b0  lwz   r9,0xf8(r31)     ; the live descriptor
822983b4  lwz   r8,0xfc(r31)
822983b8  lwz   r7,0x100(r31)
822983bc  lwz   r6,0x104(r31)
822983d0  lwz   r11,0x50(r1)
822983d4  cmplw cr6,r11,r10      ; are we in state 0x82297540 ?
822983d8  bne   cr6,0x82298410
```

## Established — the route to the push

`0x822A23D8` is reached through **`0x82296E40`**, whose call site is
`0x82296FAC`, and every install is preceded by `bl 0x82296e40` in the same basic
block. `0x82297540`'s own tick additionally **inlines an equivalent broadcast** —
same `[unit+0xD8]` array, same `[unit+0xDC]` count, same `li r4,0x7d4`, same
virtual slot `+0x24` — reached only on the `-2` tick.

`0x822982C0` (slot `+0x40`, 51 instructions, complete) does both in one call:
inline SetInitialState to `0x82297B20`, enter it, `bl 0x82296E40` — **the
placement** — then SetState to `0x82297540`.

## Where the gap is now

It has moved, and that is the result:

- **was**: what starts the leader's FSM.
- **now**: who writes `[unit+0xD8]` / `[unit+0xDC]` to non-zero, and who invokes
  slot `+0x38` or `+0x40` after children are attached.

An image-wide scan for `lwz rT,0x40(rA)` / `mtctr` / `bctrl` finds **140**
dispatch sites and none was type-narrowed; two spot-checks turned out to be a
different class. The `stw …,0xd8(rX)` scan finds only the zeroing, and it misses
`stwx` and precomputed-base forms, so **it is not an exhaustiveness claim** — the
delegated report said so itself.

## The instrument control worth keeping

Every function the report drew a conclusion from was checked with `.pdata`'s
`FunctionLen` against the `exports/` row count:

```
82295030 27=27   822950A0 39=39   82296E40 205=205  82297540 256=256
822980C8 49=49   822982C0 51=51   82298390 73=73    822986B0 106=106
8229ADF8 97 vs 26   822A23D8 460 vs 6   8229C920 278 vs 214
```

The three that disagree are the VMX128 functions downstream, and no claim was
made about their bodies beyond address containment. That is the right shape:
**the listing's completeness measured against an independent record of the
function's length, before any conclusion rests on it.**

## Not established

- The last hop into `0x822982C0` or `0x822980C8` from the mission loader.
- What writes the child count.
- Whether the inlined `0x7D4` loop in `0x82297540` and `0x822A23D8`'s loop are
  identical in effect; only the caller side was read.
