# Cycle 1235 — the state waits for the load it starts, and I stopped one instruction short again

## The answer, and it refutes cycle 1218's reading

Cycle 1218 said that if nothing clears the gate byte, the FSM transition "fires on
the first UPDATE the state receives" and the mission load is unconditional once
the state is entered. **Something clears it, and it is the state's own ENTER
arm.**

```
8219a114  lwz   r11,-0x45f0(r11)   ; the CTaskModeManager
8219a118  addis r3,r11,0x16
8219a11c  subi  r3,r3,0x56c4       ; + 0x15A93C — the CTaskLoading sub-object
8219a120  bl    0x821b8318         ; BeginLoad
```

and inside:

```
821b8334  li   r30,0x0             ; the only definition of r30 in the function
821b8408  beq  cr6,0x821b8424      ; skip on failure
821b8418  stb  r30,0xa(r31)        ; the byte := 0
821b841c  stb  r11,0x9(r31)        ; r11 = 1
821b8420  stw  r11,0xc(r31)        ; state := loading
```

Read here, all of it. `r30` is non-volatile, so the five intervening calls
preserve the zero.

**So `0x82199F68` is a load-wait state.** ENTER starts the load and clears the
flag; UPDATE reads it and transitions only when it returns to 1, which
`0x821B8430` (Poll) does at `821b84a8` once the resource resolves. `[0x82A21A9C]`
is the elapsed-frame counter — zeroed in ENTER, incremented in UPDATE.

The protocol on the byte: **`1` = idle or complete, `0` = load in flight.** Four
writers, all proven to be on this object by their address-forming instructions.

## I stopped one instruction short. Again.

Cycle 1223 examined `0x8218D2A0`, correctly called it a flag **setter**, and
concluded *"it sets; it does not clear."* The identical **clearer** is the next
function, `0x8218D2C8`, eight instructions later — `li r11,0x0` where the setter
has `li r11,0x1`:

```
8218d2c8  rlwinm r10,r4,0x0,0x1f,0x1f
8218d2cc  li     r11,0x0
8218d2d8  stb    r11,0x9(r3)
8218d2dc  rlwinm r10,r4,0x0,0x1e,0x1e
8218d2e4  beqlr  cr6
```

My cycle-1223 dump ended at `8218d2c4 blr`. **One instruction before the
counter-example to my own question.**

That is the third time this session: cycle 1214 stopped short of a back-edge,
cycle 1224 stopped one basic block short of a `cmpwi r4,2` guard that made its
conclusion impossible, and cycle 1223 stopped at a `blr` immediately before the
function that answers it. **Each time the missing instruction was within twenty
bytes, and each time the stopping point was a natural-looking boundary** — a
`blr`, a return, the end of a dump.

It happens not to change today's answer, because neither `0x8218D2A0` nor
`0x8218D2C8` is on `CTaskLoading`'s vtable. The read was short, not wrong. But
"short and lucky" is not a standard.

## The control that could have overturned the finding

If `0x8218D2C8` — a generic clearer of somebody's `+0x0A` — were reachable on
`CTaskLoading`, the answer would have been "the generic clearer does it" and the
whole load-wait reading would collapse.

Searched as a **data word**: 83 aligned occurrences in `.rdata`, and
`CTaskLoading`'s slot 9 (`0x82065A48`) is **not one of them** — that slot holds
`0x822DDBE8`, which disassembles to `blr`. A text scan for calls to the three
functions returns **0 hits in 851,718 instructions**; they are vtable-only.

**Scored 0 of 83 and 0 of 851,718.** A rival that had every opportunity to win.

## All 65 stores accounted for

Cycle 1223 found 65 stores at displacement `0xA` and resolved one. All 65 are now
partitioned, mechanically, with no residue either way: **4** on the object, **1**
on a sibling member `0x13C` later, **3** on `CModeTaskBase` at the same offset —
the collision — and **57** ruled out by their neighbours: allocator block headers
with a `0x10` stride, telemetry records whose `+0x00` is a small literal tag and
`+0x04` an `stfs`, string-layout halfword runs, scene-node flag words,
`SYSTEMTIME` copies.

## A false negative worth recording

The first search looked for `0x15a9…` as a **displacement** and returned zero.
Worthless: the gate byte is never reached by a 16-bit displacement. Every access
is `lis rX,0x15` / `ori rX,rX,0xa946` / `lbzx` — a **split constant with indexed
addressing** — and the object pointer likewise (`addis …,0x16` + `subi …,0x56c4`).
Exactly four such `ori`s exist and all four are followed by `lbzx`.

This is the same shape as cycle 1234's `addi`+`lwzx`: **a scan for one addressing
form is blind to the other**, and neither form is exotic.

## Not established, stated plainly

- **57 of the 65 were ruled out structurally, not numerically.** Their base
  registers were not individually resolved to a constant; the argument rests on a
  census of address-producing forms plus a propagation closure.
- **A bulk `memset`/`memcpy` over the manager** would take the byte with it and
  was not scanned. The sub-object pointer never reaches such a call; the manager
  pointer does.
- **The clear is conditional.** `821b8408` skips it when `0x821B8050` /
  `0x821B8218` / `0x821B7DC8` — selected by `[0x826E4EB4]+0x78`, the same mode
  word that picks the loader — returns null. On that path the byte keeps its
  value and the transition fires on the next UPDATE after all. **Which of the
  three runs for Mission 01, and whether it can fail, is not established.**
- An array write inside the manager landing on `+0x15A946` is not excluded.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
the clear, the ENTER call and 0x8218D2C8 re-read here
```

No product code changed.
