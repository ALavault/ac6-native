# AC6 retail NTSC-U/J — the predicate question connects to the already-known interrupt-callback gap, not an independent unknown (r99)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle — investigation only, for
the reason explained below.

## Continuing r98's frontier

r98 left r97's predicate question as the sole remaining live blocker.
Before accepting or rejecting the "predication disabled → execute
unconditionally" interpretation r97 declined to guess, this cycle traced
both of the buffer's two predicated packets to their real construction
sites, the same method used for opcodes `0x45`/`0x46` in r98.

## `DRAW_INDX_2`'s predicate bit: hardcoded

`FindInstructionScalar.java 0x3601` found the construction of the exact
captured header (`0xC0003601`) at `0x821e14a0`, inside `Function_821E1248`.
The value is built as a **fixed compile-time constant**
(`lis r10,-0x4000; ori r10,r10,0x3601`, stored unconditionally a few
instructions later) — no branch or register combination anywhere near
this site touches the predicate bit specifically. For this call site, the
predicate bit is always `1`, never computed.

## `WAIT_REG_MEM`'s predicate bit: genuinely conditional, and tied to the interrupt-callback path

`FindInstructionScalar.java 0x3c01` found the construction of the second
captured predicated header (`0xC0043C01`) at `0x821e637c`, inside
`Function_821E6280` (`0x821e6280`-`0x821e63ec`) — in the same `0x821E6xxx`
address cluster as `sub_821E60A8`/`sub_821E63F0`/`sub_821E64A8` already
fully characterized in r85-r94. Here the predicate bit is **not**
hardcoded:

```
0x821e6368  rlwinm. r10,r5,0x0,0x1d,0x1d   ; test bit 2 (mask 0x4) of arg r5
0x821e636c  beq 0x821e63e0                  ; if clear, skip the predicated packet entirely
0x821e6370  lbz r10,0x2abf(r11)             ; only reached when the bit is set
...
0x821e637c  ori r8,r8,0x3c01                ; construct the predicated header
...
0x821e6394  lis r10,0xbad; ori r31,r10,0xf00d  ; the exact 0xBADF00D poison-guard constant from r88
0x821e63a8  lwz r11,0x2a94(r11)              ; the exact nested-callback block from r87/r88
```

The **alternate branch** (`0x821e63e0`, taken when the flag is clear)
does nothing further — it returns immediately (`or r3,r9,r9; blr`). The
very next instruction after that `blr` (`0x821e63f0`) is the entry point
of **`sub_821E63F0` itself** — the interrupt handler function r87/r88
already disassembled in full (confirmed by matching structure: the same
`object+0x2a94` load, the same `0xBADF00D` compare, the same `+0x10`
nested-callback-pointer read at `0x821e6414`).

## What this means

This is not two independent occurrences of hardware predication. The
`WAIT_REG_MEM` predicate path is **directly adjacent to, and reads the
same fields as, the interrupt-callback subsystem** r87-r89 already spent
five cycles fully characterizing: the native `VdSetGraphicsInterruptCallback`
stub is a no-op, the callback is registered but never invoked, and the
nested-callback pointer at `object+0x2a94+0x10` is null by design for the
`0x10001a00` object. Whatever `Function_821E6280`'s caller passes as `r5`
bit 2 — plausibly reflecting "was a graphics interrupt callback
registered" — the predicated `WAIT_REG_MEM` this content emits is
entangled with that same, already-documented gap, not a separate,
independent unknown.

**This changes the risk calculus from r97, in the direction of more
caution, not less.** A blanket "predication disabled, execute
unconditionally" fix — the architecturally plausible interpretation r97
already declined to implement without a verified source — would now also
risk masking or interacting incorrectly with the interrupt-callback gap
specifically, since this exact `WAIT_REG_MEM` construction path is one of
the places that gap's downstream effects surface. Implementing it without
understanding *that* connection first would be strictly worse than r97's
original caution.

## Decision

Still declined to implement a predicate fix. The evidence gathered this
cycle sharpens *why* r97's refusal was correct rather than resolving the
question: one of the two predicated packets in this content has provably
non-trivial, condition-gated intent tied to machinery this project has
already spent five cycles (r85-r89) establishing does not currently work
as the guest expects. Guessing a uniform bypass risks compounding that
gap rather than isolating it.

## Gates

- No native code changed; `ctest`/pytest not re-run.
- `audit_ac6_mission01_native_gate.py`: unchanged from r90-r98 (same
  pre-existing, unrelated N2 mismatch).

## Next

Two more precisely scoped options than r97's, given this cycle's
findings:
1. Trace `Function_821E6280`'s caller to find what determines `r5` bit 2
   for this specific content, and whether it correlates with the
   already-confirmed interrupt-callback registration (r87) — a bounded,
   verifiable static question, not a guess about hardware semantics.
2. `DRAW_INDX_2`'s predicate bit, being a hardcoded constant with no
   computed condition at its own site, is the lower-risk of the two to
   treat as "always execute" if a narrow, opcode-specific fix (rather
   than a blanket one) is wanted — though this alone does not unblock the
   buffer, since `WAIT_REG_MEM`'s predicated packet appears later in the
   same stream and remains unresolved.

r94's other open thread (identifying the object behind the
`sub_821E6AC8`/`sub_821F03B0` wait) also remains untouched.
