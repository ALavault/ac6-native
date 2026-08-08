# Cycle 1254 — the player is a class byte, not a Set index

## Qualification

Ghidra project `ghidra-projects-xenon/ac6-xenon` (scratch; decodes VMX128, has
no reference database). `default.xex` SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`. Flat image
`analysis-input/ACE6_X360.exe`, file offset = VA − 0x82000000. Mission 01 payload
`reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin`.
**No oracle pass was spent.** The investigation was delegated; every load-bearing
instruction below was re-read here before publication, and one of the four
re-reads failed on the first attempt — see *the instrument* .

## The question, and why it was blocking

Task JV 2b has stood on one sentence since cycle 1206: *"Set 0 is the player's
Set"* is **convergent, not derived**. The initial world position
`(-2025.0, 1500.0, 1345.0)` at container offset `0x510` is read out of Set 0. If
Set 0 is not the player's Set, the position is wrong, and cycle 1244 had already
proved the write must happen at first update rather than at load — so the
product change was gated on an identification nobody had read off an
instruction.

## Established

**The identification is by the Set's class byte. It is never by the Set index.**

`0x820A7070` is the unit constructor — and `0x820A7420`, the address the contract
named, is **inside** it. `.pdata` (flat image, file offset `0x79E00`, 8,246
entries) puts the entry boundary at `0x820A7070`, the next at `0x820A7EB0`.

The outer loop walks Sets at `r26 = [arg2+4] + r21*0xC`. Each Set's resolved data
payload is `[r26+0]`, and:

```
820a72a8  lwz    r11,0x0(r26)      ; the payload
820a72c0  lbz    r11,0x8(r11)      ; the class byte
820a72cc  cmplwi cr6,r11,0x4
820a72d0  bgt    cr6,0x820a7330    ; > 4 -> r15 = 0, r14 = 0
820a72d4  lis    r12,-0x7df6
820a72d8  addi   r12,r12,0x72ec    ; the table is at 0x820A72EC
```

| class byte | arm | `r15` | `r14` |
|---|---|---|---|
| **0** | `820a7300` | **1** | 1 |
| 1 | `820a7308` | 4 | 1 |
| 2 | `820a7310` | 4 | 2 |
| 3 | `820a731c` | 4 | `0x80000000` |
| 4 | `820a7328` | 3 | 1 |

`r15 == 1` selects factory slot `+0x10` on `CX360UnitManager`
(`820a7630 lwz r11,0x10(r11)` / `820a7638 bctrl`) = `0x820A7F48`, which calls
`0x822A6560`, which installs a vtable:

```
822a6578  lis  r11,-0x7dfb
822a6580  addi r11,r11,0x68d4     ; 0x820568D4
822a658c  stw  r11,0x0(r31)
```

RTTI read from the flat image at `0x820568D4 − 4` → locator `0x8206E2A4` →
type descriptor `0x8268FD60` → **`.?AVCAce6UnitPlayer@ACE6@@`**. The sibling
`0x82056934` resolves the same way to **`.?AVCAce6UnitOtherPlayer@ACE6@@`**.

**So a campaign mission's player unit is the Set whose payload byte `+0x08` is
`0`** — and only its first child, because the second factory's argument `r30`
equals `r15` only when `r24 == 0`.

### The Set index is an output of the identification, not an input

Disassembled **912 of 912 instructions of `0x820A7070`, zero gaps** (verified
programmatically against the `.pdata` extent). `r21`, the Set index, occurs
thirteen times. Exactly two are comparisons:

- `820a7704 cmplwi cr6,r21,0x5` — but `820a76e8 subi r21,r15,0x1` has already
  overwritten the register with the `r15 − 1` table index. This compare is not
  about the Set index at all.
- `820a7e44 cmpw cr6,r21,r11` — the loop bound, register against register.

The Set index is restored from its spill at `820a7c38 lwz r21,0x54(r1)` and
**written out**, never tested:

```
820a7648  stw r21,0xd0(r16)      ; on the CAce6Unit
820a7a0c  lwz r10,0x54(r1)
820a7a28  stw r10,0x170(r31)     ; on the galib::CGaObj
```

For Mission 01 `+0x170` is `0` — as an *effect* of the class-0 Set happening to
be record 0, not as a cause.

### The rival, and the corpus that separates it

**R:** the player is Set index 0. **H:** the player is the class-byte-0 Set.

- **Near corpus.** Mission 01's 230 Set records: class histogram
  `{0:1, 1:40, 2:188, 4:1}`. H predicts exactly one `0`; it held. R predicts
  nothing and so is not tested by it.
- **Far corpus, and this is the control that could have failed.** Every
  `DATA.TBL` entry in 0..119 with expanded size ≥ 200 KB whose FHM child 0 carries
  magic `00 00 00 10`: **38 scenario containers, 4,591 Set records.** Class byte
  values over the whole corpus are `{0:324, 1:904, 2:3328, 3:18, 4:17}` —
  **nothing outside the switch's 0..4 domain in 4,591 records**, which is
  simultaneously a control on whether the right offset is being read, since a
  wrong offset yields values above 4.
  - All **15 campaign containers** (entries 9..23): exactly one class-0 record,
    always at index 0.
  - The 23 non-campaign containers: class-0 counts of 1 (×7), 4 (×2), 10 (×2),
    14 (×1), 16 (×5), 30 (×6).
  - In entries **41, 42, 44, 45** the class-0 records are **not** a `0..n−1`
    prefix — entry 41 has them at 0..14 *and* 24..38.

R cannot survive 30 class-0 Sets at non-contiguous indices. The control could
have come back "one class-0 record at index 0 everywhere", which would have left
R and H inseparable. It did not.

## Not established, stated as plainly

- **"Set 0 is the player's Set" is still not a code rule.** The code rule is
  *class byte 0*. Set 0 satisfies it in 15 of 15 campaign containers, which is a
  property of the authored data. The position now rests on `820a72c0`, a read
  instruction, rather than on convergence — that is what changes.
- **No instruction was read that stores the campaign manager at
  `*(0x826E4EB4) + 0x29C80`.** That object's identity rests on its slot `+0x04`
  value set matching the constructor's tests and being exhausted by the
  `CAce6MissionManager` family. If a campaign mission ran under the base class,
  the category would be 0 and nothing above changes.
- `r20`, the constructor's third argument, is unidentified; it gates the
  `CAce6Unit` creation at `820a7618`.

## Corrections

- **Cycle 1206** wrote that byte `+0x08` "reads 0 for all 230 Sets in the slot-0
  container". It reads `0` for exactly **one** of the 230. Cycle 1206 was
  measuring `node+0x08` rather than the resolved payload's `+0x08`.
- **Cycle 1206's `+0x61 = +0x62 = 0xFF`** is weaker than it was presented as. The
  DPL path never reads `+0x61`, so `0xFF` is *consistent with* that path, not
  evidence for it.
- **Cycle 1178's `global+0x4B40`** is read at `82097FC4`, **after** the
  `bl 0x820a7070` at `82097F84`. It is not an argument to the constructor and
  takes no part in identifying the player.

## A product discrepancy, found and not yet fixed

`build_units` in `reconstruction/ace-combat-6/src/retail_mission_state.cpp`
increments its ordinals for **every** record whose faction side code is 0 or 3.
Retail increments only for **class-0** records (`820a748c`, `820a7550`). The two
rules give the same answer on Mission 01 — one class-0 record, at index 0 — and
different answers anywhere else. Recorded here rather than fixed in the same
cycle, because the fix is unobservable under Mission 01's data and would ship
without a control.

## The instrument

`Ac6XenonDisasm`'s arguments after the output file are **all start addresses**.
A verification run here passed `820A7070 820A7EB0` intending the range, received
two 300-instruction blocks — the second beginning past the function's end — and
covered **300 of 912 instructions**. The listing ended on an ordinary instruction
with no marker, so a negative over a third of a function looked exhaustive. The
claim under test survived the corrected 912/912 read, which is luck.

The script now prints a trailer per block naming the count and whether the
300-instruction cap truncated it, following the rule `Ac6XenonForceScan` already
keeps: a scan states its own denominator.

Two further instrument notes carried from the investigation:

- A force scan for `0x29c80` returns **0**, and that zero is void: the offset is
  materialised as `lis r11,0x2; ori r19,r11,0x9c80`. The same scan for `0x9c80`
  returns 290 hits.
- The jump tables inside `0x820A7070` disassemble as `lwz r16,0xXXXX(r10)`, whose
  encoding *is* the target `0x820A_XXXX` — self-checking once seen, and easy to
  read as code.

## Decisions taken

- **JV 2b unblocks**, and the product change stays exactly as cycle 1244 proved
  it: the position is applied at **first update**, in the session loop, never as
  a load-time write.
- The `build_units` ordinal rule is filed, not fixed, for want of a control.
- The delegated report's corpus work was accepted; its four load-bearing
  instruction claims were re-read here first, and the negative among them was
  re-run after the instrument was found to have sampled a third of the function.
