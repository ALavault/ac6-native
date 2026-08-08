# Cycle 1255 — first-wins, derived twice, and the data question it does not settle

## Qualification

Canonical Ghidra project `ghidra-projects/ace-combat-6` for the reference
database; scratch `ghidra-projects-xenon/ac6-xenon` for re-reading instructions.
`default.xex` SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
Flat image `analysis-input/ACE6_X360.exe`, 7,502,848 bytes, VA = offset + 0x82000000.
**No oracle pass was spent.** Delegated investigation; every load-bearing claim
below was re-read here, and the exhaustiveness negative was re-run with an
instrument that does not involve Ghidra at all.

## Established — the policy is first-wins, and it is enforced twice

### The map is 101 buckets of binary search trees

`registry+0x100` is 101 hash buckets, each holding an unbalanced BST of
`0x10`-byte nodes — not a chain, not open addressing, not a sorted array. Node
layout: `+0x00` child for greater keys, `+0x04` child for less-or-equal, `+0x08`
key, `+0x0C` value. Three functions touch it: **find `0x8234CCE0`**, **insert
`0x8234CDC0`**, **erase `0x8234CE70`**.

### The insert refuses a key that is already present

The descent tracks a slot, with the sentinel's key pre-stamped at `map+0x18` so
the walk always terminates. Re-read here:

```
8234ce24  lwz   r11,0x0(r9)      ; the node the descent landed on
8234ce28  addi  r8,r10,0x10      ; the sentinel
8234ce2c  cmplw cr6,r11,r8
8234ce30  bne   cr6,0x8234cdd4   ; not the sentinel => key present => bail
...
8234cdd4  li    r3,0x0
8234cdd8  blr
```

It returns 0 and writes nothing. The value store `8234ce5c stw r5,0xc(r11)` is on
the fresh-node path only, where `r11` came off the free list at
`8234ce34 lwz r11,0x4(r10)` — never the incumbent. **No overwrite, no chain
append, no second node with an equal key can exist**, so the lookup has no
collision chain whose end could matter.

### The caller guards as well

`0x8234BEC8`, the per-resource registration called once per texture by the NTXR
mount loop `0x82340870`:

```
8234bf10  subi  r28,r11,0x7f00   ; 0x828C8100
8234bf20  bl    0x8233ebb0       ; find; 1 iff absent
8234bf24  cmpwi r3,0x0
8234bf28  beq   0x8234bf54       ; present => never reaches the insert
```

### Exhaustiveness, measured outside Ghidra

The negative that carries the argument — that no other code path inserts — was
re-run by decoding every 4-byte word of the flat image directly, matching the
PowerPC `bl` form (opcode 18, `AA=0`, `LK=1`) and resolving each target:

- `bl 0x8234CDC0`: **exactly 2**, at `0x82343320` and `0x8234AEB8`. Agrees with
  the reference database.
- the word `0x8234CDC0` as aligned data: **0**. It is in no vtable and no
  dispatch table, so there is no indirect insert.

This instrument shares no code with Ghidra's analysis, which is the point: the
same negative from the same tool that produced the claim is not a control.

### Where the shader registry differs, and where it does not

`0x828CCB80` reaches the same `0x8234CDC0` via `0x8233F250` → `0x8234BDD8` →
`0x8234AE78`, but with **no find-guard** — it creates unconditionally and
discards the insert's return at `8234be3c`. A duplicate there leaks the new
object while the map keeps the incumbent. **Identity is still first-wins.**

## Three qualifications that must be carried into the port

**(a) Duplicates are not impossible, and the mechanism that would prevent them
is off in Mission 01.** The mount loop biases small ids before registering:

```
82340914  lis   r11,0x1000
82340918  cmplw cr6,r5,r11
8234091c  bge   cr6,0x82340928   ; id >= 0x10000000 -> no bias
82340920  lwz   r11,0x8(r27)     ; the per-archive bias
82340924  add   r5,r11,r5
```

Ids below `0x10000000` get the per-archive bias; GIDX-form ids never do. The
established measurement is that the bias at `[0x828C9700+0x08]` is **zero** in
Mission 01, so it separates nothing and two packs repeating a small id do
collide.

**(b) Identity is first-wins; content is "first mount that supplies data".** On
the present branch `0x8234BEC8` interrogates the incumbent and, only if it
reports state `-1`, re-fills it in place from the *later* pack
(`8234bf78 cmpwi cr6,r11,-0x1`, `8234bf90 bl 0x8233eb20`, which reaches a find
and not an insert). The object a duplicated id binds to is the first
registrant's permanently; its bytes may come from a later pack if the first left
a placeholder.

**(c) First-wins holds among currently live entries.** `0x8234A950` decrements a
refcount and at zero reaches the erase `0x8234CE70`, returning the node to the
free list; a fully released id can be re-inserted later.

## Not established — and this is the half the cycle did not close

**Whether any id is actually duplicated across the packs Mission 01 mounts.**
The code answer makes the flat extraction safe *provided no duplicate exists*;
cycle 1248 counted **847 duplicates over 205 ids across 1052 packs**, so on its
face the condition fails — but that count was produced by a script that was not
preserved, and no committed artefact carries an id.

I tried to recompute it and stopped, because the instrument was not measuring
one thing. Under one extraction root, `runtime_idx_*` excluded, there are **346
NTXR wrappers, all declaring entry count 1**. Locating the id by "the first word
at or above `0x10000000`" — the mount code's own threshold — returns **four
different offsets**: 64 (165 files), 96 (117), 112 (55), 80 (2) and 36 (7). A
locator that lands in five places is finding a size or a flag as often as an id.
The calibration wrapper `exports.pre-s0/first-linked-0x10002215.ntxr` carries its
id at offset 88 and declares **6** entries, so it does not settle the layout for
the count-1 files either.

**The next step is named rather than guessed**: derive the per-entry record
layout from the NTXR reader `0x8234B300`, which already checks the magic, and
read the id at the offset that reader uses. Then the duplicate count is a
one-line group-by, and with it the question of whether mount order is
load-bearing for Mission 01 becomes a fact instead of a worry.

## Corrections

- **Cycle 1248** listed duplicate-mount policy under "not established" and wrote
  in the same paragraph that "the first mount of an id wins". That was right
  about the guard at `0x8234BEC8` and did not know the insert refuses
  independently. The policy is now derived; what stays open is its
  *applicability*, which is the data question above and a different one.
- **`MISSION01_LADDER.md`** carried "first mount of an id wins" inside a
  parenthesis of an item listed as open. Cycle 1254's commit removed it as a
  guess wearing a result's grammar. It was, in fact, correct — and removing it
  was still right, because at that moment nothing in the repository derived it.
  A true sentence with no derivation behind it is not evidence, and the fix is
  to derive it, which is what this cycle did.

## Instrument notes

- `exports.pre-s0/` is untrustworthy for this kind of question. Its `callers`
  arrays reported **zero** callers for `0x8234CDC0` where the reference database
  and the byte scan both find two, and it truncates functions with
  `__savegprlr` prologues — `0x8234AE78` comes back as two instructions.
- A "first word above a threshold" locator is not an offset derivation. It
  returned five distinct offsets over 346 files here, and each one looks
  plausible in isolation.
