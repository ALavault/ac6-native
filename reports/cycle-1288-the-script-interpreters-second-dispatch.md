# Cycle 1288 — the script interpreter's second dispatch

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; instructions re-read in
`ghidra-projects-xenon/ac6-xenon`. `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## Why

Cycle 1285 found that command opcode 30 — arm 28 of the mission-script
interpreter `0x8225A600` — is the **only** invoker of the virtual slot that
starts the Set leader's FSM and places its children. It also reported, honestly,
that `count_indirect_branches.py` said `bctr=2` and only the first had been
decoded, *"which is exactly the failure that tool exists to catch."*

This reads the second.

## Established

Both dispatches, with their tables built by `subi` — the negative-immediate form
of `addi`, which the counter recovers correctly:

**First**, the opcode switch:

```
8225a740  cmplwi cr6,r11,0x21        ; 33, so 34 arms
8225a744  bgt    cr6,0x8225afe0
8225a748  lis    r12,-0x7dda
8225a74c  subi   r12,r12,0x58a0      ; table 0x8225A760
8225a75c  bctr
```

on `r11 = [record+0x04] − 2`. Arm 28 is opcode 30, the placement command.

**Second**, and it is not an opcode switch at all:

```
8225ad40  lhz    r10,0x8(r31)        ; a HALFWORD at record+0x08
8225ad44  extsh  r10,r10
8225ad4c  cmplwi cr6,r10,0x7         ; 7, so 8 arms
8225ad80  lis    r12,-0x7dda
8225ad84  subi   r12,r12,0x5268      ; table 0x8225AD98
8225ad94  bctr
```

Its first six entries: `0x8225AF04`, `0x8225AF04`, `0x8225ADCC`, `0x8225ADCC`,
`0x8225ADCC`, `0x8225ADCC` — arms sharing targets in pairs and runs.

So the interpreter has **two independent selectors**: the command opcode at
`record+0x04`, and a signed halfword at `record+0x08` that a later arm
sub-dispatches on. `0x8225AD94` sits well past arm 28's body at `0x8225A8E0`, so
this sub-dispatch belongs to a different opcode, not to the placement one.

## What it does and does not change for the placement question

**It does not touch arm 28.** Command 30's path to `0x822982C0` is unaffected,
and the question of whether command 30 fires during Mission 01 is exactly where
cycle 1285 left it.

**It closes the instrument's own complaint.** The tool reported two dispatches;
both are now read, and the second turned out to be a different question rather
than a missed half of the same one. That is the useful outcome of counting
first: the unread `bctr` was neither a threat to the finding nor nothing — it
was a second selector on the same records, which is worth knowing before anyone
reads a script record's layout.

## Not established

- **What `record+0x08` selects**, beyond its arity of 8 and the shared targets.
  None of the eight arms was read.
- Which opcode arm reaches `0x8225AD94`. It is past arm 28 in address order,
  which orders the code and not the dispatch.
- Everything in task #17 remains open: no Mission 01 script record with opcode
  30 has been found, and the climb above the interpreter still stops at three
  FSM state handlers whose scheduling is untraced.
