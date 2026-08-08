# Cycle 1264 — one duplicate settled as an artefact, the other still open

## Qualification

Delegated investigation on the canonical project; **every load-bearing
instruction below was re-read here** in `ghidra-projects-xenon/ac6-xenon`.
`default.xex` SHA-256 `acc302c1…11bcde`. **No oracle pass was spent** — the
`cycle-739` corpus was used as a file-side control on which PAC entries a
Mission 01 boot touched, and no behaviour was read out of recomp logs.

## The chain, and the mode at its end

```
0x82199F68  -> 0x8219A120 bl 0x821B8318
            -> 0x821B83FC bl 0x821B7DC8
            -> 0x821B7DD8 bl 0x821B7960          the mission resource loader
            -> 0x821B7B74 / 0x821B7B94 bl 0x821D2050
            -> vtable 0x82067B9C slot 5 = 0x821D1620
            -> 0x821D16C8 bl 0x82335F18 -> 0x82340870
```

Reachability was computed from a `bl`-opcode scan of the flat image rather than
asserted: the reverse closure of `0x821D2050` has roots
`{0x82145A48, 0x82199F68, 0x821B99B8}`, and `0x82199F68` is on the established
Mission 01 chain.

**Both Mission 01 sites mount mode 0**, re-read here:

```
821b7b58  li r9,0xee
821b7b5c  li r8,0x0        <- MODE 0
821b7b60  li r7,0x0        <- id base 0
821b7b64  li r6,0x0        <- child index 0
821b7b74  bl 0x821d2050
821b7b7c  li r8,0x0        <- MODE 0
821b7b84  li r6,0x1        <- child index 1
821b7b94  bl 0x821d2050
```

and `0x821D2050` stores that argument as the task's mode:

```
821d209c  stw r28,0x14(r11)    ; r7 -> id base
821d20a0  stw r27,0x18(r11)    ; r8 -> MODE
```

### A refinement to my own cycle 1260

Cycle 1260 wrote that "`mode` is the fourth argument of the mount loop
`0x82340870`". That is true **of the loop** and misleading **at a call site**,
because nobody calls the loop — the only entry is the thunk `0x82335F18`, which
shifts:

```
82335f24  or r6,r5,r5      ; thunk r5 -> loop r6 == mode
82335f28  or r5,r4,r4      ; thunk r4 -> loop r5 == id base
```

So at a thunk site the mode is in **`r5`**, and reading `r6` there gives the
wrong answer. The control is that reading `r5` at all 53 sites reproduces cycle
1260's published 16 / 36 / 1 split exactly; reading `r6` does not. The counts
were right; the sentence would have misled the next reader.

## Established — the 28 copies of `0x0F000000` are an artefact

Entry 9's children are mounted through `0x821D18E8`, which calls the id
allocator `0x821AEB08` per child and passes the result as the base on a
hard-wired mode-1 mount. The allocator, re-read here:

```
821aeb40  lis   r11,0xf00        ; 0x0F000000
821aeb44  ori   r9,r11,0xf000    ; limit 0x0F00F000
821aeb48  lis   r11,-0x7dc0
821aeb4c  lwz   r10,-0x55c4(r11) ; the counter at 0x823FAA3C
821aeb50  addi  r3,r10,0x1       ; return counter + 1
821aeb54  cmpw  cr6,r3,r9
821aeb58  stw   r3,-0x55c4(r11)
821aeb5c  blelr cr6
821aeb60  lis   r3,0xf00         ; overflow: reset to 0x0F000000
821aeb64  stw   r3,-0x55c4(r11)
```

It emits `0x0F000001`…`0x0F00F000` and resets to `0x0F000000`. **The value in
the file is the allocator's seed — a placeholder meaning "assign me" — and on a
mode-1 mount the file's id is discarded before it ever reaches the registry.**
Twenty-eight files carrying the same id is not a collision; it is twenty-eight
files saying nothing.

## Not established — the 115 copies of `0x08000000`

**PAC entries 199 and 210, which carry all 115, could not be tied to any mount
site.** Every `bl 0x821D1DD0` was enumerated — 39 sites — and the resource ids
recoverable as immediates are 165, 166, 204–208, 229, 267, 407, 534, 535.
Neither 199 (`0xC7`) nor 210 (`0xD2`) appears as an immediate anywhere on a
mount path, nor in any table read. The remaining sites take the id from a
register or a load that was not resolved.

The delegated report offered the obvious reading and then refused it, correctly:
the 115 are all-identical, nested in an FHM, and match the `0x0F000000` pattern
exactly, which *suggests* the same loop-mount — **and that is a suggestion with
no control, the same class of reasoning cycle 1260 killed.** It is recorded here
as unresolved rather than as probable.

## Also not established

- **The 190 nested NTXRs of entry 119 have no mount site.** `0x821B7B74` and
  `0x821B7B94` mount top-level children 0 and 1 only. Whatever mounts
  `021_FHM/015_FHM`'s 170 wrappers — ids `0x49e`, `0x53a`…`0x559`,
  `0x1049`…`0x1079` — was not found, so cycle 1260's unexplained consecutive
  runs stay unexplained.
- **"Mission 01 is index 1" is a corpus identification, not a derivation.** The
  index comes from `0x820943B0([0x826E4EB4]+0x70)`, a mode-dispatched field read
  whose writer was not traced. What ties it is that index 1 predicts PAC entries
  9, 119 and 165 from three separate tables, and a Mission 01 boot dumped
  exactly 9, 119, 165, 199 and 210 — while index 0 would predict 118 and 51 and
  index 3 would predict 24, none of which that boot touched. That is a strong
  agreement across three tables and it is still not a read of the writer.
- `0x82234DD0`'s semantics are inferred from the manifest agreeing with the
  code's indices, not from reading it.

## Where that leaves the duplicate question

| entry | wrappers | ids | mount |
|---|---:|---|---|
| 119, children 0 and 1 | 2 | `0x9065`, `0x9068`, distinct | **mode 0**, derived |
| 165, children 0–9 | 10 | `0x03050000`, `0x03000001`…`9` | mode 1, bases equal the file ids |
| 9, `004_FHM`…`010_FHM` | 28 | `0x0F000000` ×28 | mode 1, **artefact, settled** |
| 199 `000_FHM` | 14 | `0x08000000` ×14 | **no mount site found** |
| 210 `002_FHM` | 101 | `0x08000000` ×101 | **no mount site found** |
| 119 nested | 190 | distinct, incl. `0x1049`…`0x1079` | **no mount site found** |

Of the 141 extra copies cycle 1256 measured, **28 are now explained and 113 are
not**. The first-wins policy derived in cycle 1255 is confirmed never to fire on
the two containers whose mounts *are* derived: entry 119's two children carry
distinct ids, and entry 165's ten bases equal its ten file ids.

## Instrument notes

- Five of the 53 thunk sites have no entry in `exports/` and were hand-decoded
  from the flat image; all five read `li r5,0x0`. `exports/` remains a source of
  missing rows rather than wrong ones — which is a different failure from
  `exports.pre-s0/`, whose caller arrays were wrong.
- The flat-image `bl` scan earned its use the right way round: it independently
  reproduced all 53 thunk sites and, read at `r5`, the published 16 / 36 / 1
  split.
