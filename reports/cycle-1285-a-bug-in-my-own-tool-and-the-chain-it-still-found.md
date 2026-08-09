# Cycle 1285 — a bug in my own tool, and the chain it still found

## Qualification

Flat image `analysis-input/ACE6_X360.exe`. `default.xex` SHA-256
`acc302c1…11bcde`. **No oracle pass was spent.** No product code changed.

## The bug, found by the agent I gave the tool to

`tools/find_materialised_address.py` under-counted `ori` pairs by roughly 2.6×.
PowerPC puts the operands in different fields for the two forms:

```
addi rD,rA,SIMM   rD = bits 6..10,  rA (the SOURCE) = bits 11..15
ori  rA,rS,UIMM   rA (the DEST) = bits 11..15,  rS = bits 6..10
```

I tested bits 11..15 for both. That is right for `addi` and **backwards for
`ori`**: it demanded the `ori`'s *destination* be the `lis`'s register, when the
requirement is on its *source*. It therefore found only the register-reusing
form — `lis r10,2 ; ori r10,r10,0xD3B4` — and silently dropped every
cross-register one, such as

```
8225A64C  lis r10,0x2
8225A650  lis r11,0x8200        (unrelated, in between)
8225A654  ori r21,r10,0xD3B4    -> 0x0002D3B4
```

which is the site that mattered. Fixed; `0x0002D3B4` goes from **53 to 144**
sites.

## The check that had to come first

**The MATE negative rests on this tool.** Cycle 1276 concluded `'MATE'` is
materialised nowhere, with `'NTXR'` and `'NDXR'` as positive controls. Re-run
against the corrected scanner:

```
0x4D415445  'MATE'   0 sites
0x4E545852  'NTXR'   1 site   0x8234B304
0x4E445852  'NDXR'   1 site   0x8233EF48
0x46484D20  'FHM '   0 sites
```

**It survives.** The controls still land on their known instructions, and the
`'FHM '` zero is consistent with cycle 1246's finding that its reader checks no
magic. The bug did not reach that conclusion because a magic test loads its
constant into one register and reuses it — but nothing about cycle 1276 knew
that, and it was luck a second time. The six known materialisations of
`0x82297540` also still resolve, unchanged.

## Established — the chain, both ends named

**The one writer of the child array and count is the Mission 01 unit builder
itself**, `0x820A7070`, re-read here from the image:

```
820a7c74  915000d8   stw r10,0xd8(r16)   ; child ARRAY
820a7c78  917000dc   stw r11,0xdc(r16)   ; child COUNT
820a7c80  917000e0   stw r11,0xe0(r16)   ; order list
820a7c88  917000e4   stw r11,0xe4(r16)
```

— all four fields `0x822A2330` zeroed, refilled in the same order. The count is
the byte that bounded the child-construction loop sixty instructions earlier.

**The one invoker of slot `+0x40` is `0x8225A918`**, arm 28 of the mission-script
command interpreter `0x8225A600`, i.e. **command opcode 30**:

```
8225a900  81764eb4   lwz   r11,0x4eb4(r22)
8225a904  7d6ba82e   lwzx  r11,r11,r21     ; r21 = 0x0002D3B4, the unit manager
8225a908  7c6a582e   lwzx  r3,r10,r11      ; manager[(id+1)*4]
8225a914  81630000   lwz   r11,0x0(r3)
8225a918  816b0040   lwz   r11,0x40(r11)   ; slot +0x40 = 0x822982C0
8225a91c  7d6903a6   mtctr r11
8225a920  4e800421   bctrl
```

`(id+1)*4` is the same indexing `0x8226FEC0` wrote with. Slot `+0x38` has **no**
caller after construction: both its call sites run before the `+0xDC` write,
which is cycle 1279's mechanism with its call sites now named.

The narrowing was not a spot-check: 935 candidate loads, 285 completing to a
`bctrl`, intersected with the 117 functions materialising the registry offset,
leaving 15 — **all fifteen read**, fourteen rejected by receiver.

## Not established, and it is the whole question

**That command 30 fires during Mission 01.** No Mission 01 script record with
opcode 30 was found, and the climb above the interpreter stops at three FSM
state handlers whose scheduling was not traced. **That is what decides whether
the 135 units get placed**, and it is untouched.

Also open: that a registry entry carries the unit vtable (the link is
structural — same manager, same indexing, same fields — and no vtable store was
read); and the **second `bctr` in `0x8225A600`**, at `0x8225AD94` with table
`0x8225AD98`, which `count_indirect_branches.py` reported and the agent left
unread, noting that this is exactly the failure that tool exists to catch.

## A second instrument note

`llvm-mc --disassemble` **silently drops words it cannot decode**, shifting every
later address; a listing of `0x820A7070` came out 898 lines for 912 instructions
and mis-attributed instructions by up to 14 slots. `0x820A7070` contains 14 such
words. Any listing from that path needs a per-instruction sentinel, or
`check_listing_against_pdata.py` against it — which would have caught it as a
14-instruction shortfall.
