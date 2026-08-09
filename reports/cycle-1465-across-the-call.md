# Cycle 1465 — across the call

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py`.
- No product C++ changed; ctest stays **56**. **No contract entry.**
- `tools/ppc_read.py` gains `ld`/`std` (opcodes 58 and 62).

## Correcting cycle 1463

That cycle scanned forward from each of the 27 accessor sites and reported, for
all of them, "the pointer is handed off" — because the scan **stopped at the
first `bl`**.

For six of the 27 that is wrong. `0x82097840`/`0x82097844` and their replay and
campaign twins load both maps into **`r30` and `r29`** — callee-saved registers —
and the `bl` that follows is not what receives them. The pointer is live *across*
the call and used afterwards.

That is *stopping at a natural boundary*, indexed in `INSTRUMENT_DISCIPLINE.md`
with the note that three refutations sat within twenty bytes of where a cycle
stopped. Here it was one instruction.

## What the two functions actually are

**`0x82105738` ignores its first argument.** `r3` is overwritten at
`0x82105750` — `addi r3,r1,0xE0` — with no save. It formats a string:
`0x823326C8` takes a buffer, loads a literal at `0x82286180` and calls
`0x8233DEA8`. There is an `fsqrts` at `0x821057A8` over two float vectors from
`r4` and `r5`. A distance, printed.

Establishing that needed a decoder fix. `0x82105740` and `0x82105744` came out as
`.long` — they are `std r30,-0x18(r1)` and `std r31,-0x10(r1)`, opcode 62, which
`ppc_read.py` did not know. Two unknown words in a prologue, in exactly the place
a saved `r3` would have been, and the tool's own rule saved me: **a `.long` is a
signal to look, not a gap to ignore.**

**`0x820943B0` never receives the map at all.** It is a jump table:

```
lwz    r11,0x8(r3)        a kind
addi   r11,r11,-2
cmpliw r11,3              -> default when above
lwzx   r0,r12,r0          table at 0x820943D8
bcctr
  -> lwz r3,0x10(r3) / 0x18 / 0x20 / 0x24 ; blr
```

A getter returning one of four fields by kind. `INSTRUMENT_DISCIPLINE.md` has
this one too — *when several dispatch sites in one function share a branch
target, they are a switch and not a sequence* — and here the four arms are four
one-line getters behind one `bcctr`.

## The negative, re-established and still bounded

Scanning **600 instructions** past each of the six sites, on the register holding
the map: **no write to `+0x30`.** No `blr` was reached in 600 either, so these
are large functions and the scan is incomplete by construction.

So cycle 1463's conclusion survives its broken scan, and its bound changes: not
"the pointer is handed off before anything happens to it", but "no store to
`+0x30` on the map register within 600 instructions of any of the 27 sites".

## Not established

- The writer of `+0x30`. Two cycles of bounded negatives and no positive.
- What is at `+0x08`, `+0x10`, `+0x18`, `+0x20`, `+0x24` of whatever
  `0x820943B0` reads — a different object from the map.

## Gates

```
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 56
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Stop chasing the writer and read the consumer's other branch.** `0x82102148`
abandons on a zero `+0x30` and writes `2` to an out-parameter; `0x82102E70`, slot
80, is what reads that out-parameter and decides what to do next. Two cycles have
searched for who *fills* the field; nobody has read what happens when it is
empty, and that is one function already in hand.
