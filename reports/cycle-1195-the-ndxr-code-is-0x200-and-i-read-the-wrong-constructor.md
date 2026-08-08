# Cycle 1195 — every retail NDXR is code 0x200, and I read the other constructor

## What the control confirmed

Cycle 1194 derived from `0x8234CA28` that a recognised file's type code is the
`u16` at `+0x08`. Measured across the extracted corpus:

```
NDXR files: 537      non-NDXR magic: 0
u16 at +0x08:        {512: 537}
[+0x04] == file size: 537 of 537
```

**Unanimous, twice.** Every one of the 537 files carries `0x200` at `+0x08` —
which is exactly one of the three codes the dispatcher `0x8234CB58` handles, and
the two derivations were made independently, one from instructions and one from
files. And `+0x04` is the file's own byte length in all 537, a field cycle 1194
did not predict and which corroborates that the header is being read at the right
base.

## What the control refuted, and it was mine

Cycle 1194 ended pointing task 2g at `0x82350AF8`, the constructor the dispatcher
reaches for codes **1 and 2**. I then followed it to `0x82350B80` and to
`0x82354D68`, and read out of the latter what looked like the NDXR container
topology:

```
82354d6c  lhz r10,0xa(r3)      ; a count at +0x0A
82354d74  addi r9,r3,0x20      ; records at +0x20
82354d94  lwz r10,0x8(r9)      ; per record, a sub-count at +0x08
82354db4  lwz r8,0x14(r11)     ; per sub-record, a row count at +0x14
82354dd0  rlwinm r8,r8,0x5,...  ; stride (rows + 1) * 0x20
```

Applied to `011_NDXR.ndxr` at its file base, the walk **fails on the first
record**: `[+0x28]` is 3,203,649,340 and the second group runs past the end. The
bytes at `+0x20` are floats — `0x33 79 a9 34`, `0x40 d7 9e 7a` — not a record
header.

The reason is the census above. **No retail NDXR takes the `0x82350AF8` path**,
because none of them carries code 1 or 2. `0x8234CB58` sends `0x200` to
`0x82350CA0` / `0x82350C50` instead, after unwrapping a GIDX header if one is
present. I read a real constructor for a format this game ships none of.

Had I written that walk up as "the NDXR header layout" — which is exactly the
sentence I was composing — it would have entered the repository as a derivation
with retail addresses attached, and every address in it would have been correct.
That is the cycle-1193 shape again, four cycles later: **live instructions, dead
path.** The difference is that this time the control ran before the commit rather
than after.

## Corrected, and standing

`0x82352B88` **is** on the NDXR path after all, contrary to how I phrased cycle
1194's title. `0x82350AF8` calls it at `82350b2c`, after installing vtable
`0x820127B4` at `this+0x00` and the size at `this+0x08`. Cycle 1194 was right
that it parses nothing — it sequences vtable slots `+0x18`, `+0x10`, `+0x20` —
but wrong to imply the address was irrelevant. It is the load sequencer; task 2g
named it for a real reason and described it wrongly.

Also standing, unaffected: the recogniser at `0x8234CA28`, both magic predicates,
and the finding that a GIDX file is a `0x10`-byte header in front of an NDXR.

## Not established, stated plainly

- The `0x200` header layout. It is `0x82350CA0` / `0x82350C50` that read it, and
  I have not opened either.
- Whether `0x82350AF8` is dead in the whole image or merely unused by NDXR. The
  census covers the extracted NDXR corpus, not every file the game loads through
  the dispatcher.
- The meaning of `+0x0A` = 12, `+0x0C`, and the three offsets at `+0x10`,
  `+0x14`, `+0x18` (`0x0D20`, `0x04D0`, `0x2960` in the sampled file). They are
  file measurements and no instruction has been shown to read them.

## Decided rather than asked

Task 2g moves to `0x82350CA0` and `0x82350C50`. I am not writing the `0x82354D68`
walk into any report as a layout, only as the finding that it is unreachable for
this content.

The two census numbers are cheap and unanimous, so they go in as a standing
control: **any future claim about an NDXR header must survive `+0x04 == filesize`
and `+0x08 == 0x200` on all 537.**

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
537/537 NDXR: [+0x04] == filesize, [+0x08] == 0x200
```

No product code changed.
