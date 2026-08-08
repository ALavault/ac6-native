# Cycle 1197 — the record array is fixed `0x30`, and the file is relocated in place

## The walk

`0x82350F08` ends by calling `0x823556E0(fileBase, this+0x10)`:

```
823556f8  lhz  r30,0xa(r31)   ; count = u16 at file +0x0A
823556fc  addi r3,r31,0x30    ; the record array begins at file + 0x30
82355710  bl   0x823555d0     ; per record; r3 for the next iteration is its return
```

and `0x823555D0` is the record visitor:

```
823555e8  lhz  r9,0x26(r31)             ; flags = rec+0x26
823555ec  rlwinm. r11,r9,0x0,0x0,0x10   ; already relocated?
823555f0  bne  0x8235562c               ;   then skip the fixups
82355610  add  r11,r11,r10
82355614  stw  r11,0x20(r31)            ; rec+0x20 += [obj+0x90]
8235561c  ori  r10,r9,0x8000
82355620  add  r11,r11,r28
82355624  sth  r10,0x26(r31)            ; mark relocated
82355628  stw  r11,0x2c(r31)            ; rec+0x2C += fileBase
8235562c  lhz  r30,0x2a(r31)            ; sub-count = rec+0x2A
82355630  lwz  r3,0x2c(r31)             ; sub array, now absolute
82355644  bl   0x82355468               ; per sub-record
82355650  addi r3,r31,0x30              ; the next record is rec + 0x30
```

**The top-level record stride is a fixed `0x30`**, and the sub-records are not
inline: `rec+0x2C` is a file-relative offset that the visitor converts to an
absolute pointer in place.

**This loader mutates the file buffer.** The `0x8000` bit at `rec+0x26` is a
"already fixed up" guard, which is why it must be — the same buffer can be
visited twice. Any port has to decide whether to relocate or to keep offsets and
add the base on use; retail chose to relocate, and a reader that keeps the file
immutable is a deviation to be written down rather than assumed.

## The control

Three predictions, each able to fail, on all 537 files:

| prediction | result |
|---|---|
| `count` records of `0x30` from `file+0x30` fit inside the file | **537 / 537** |
| `rec+0x26` bit `0x8000` is **clear** on disk for every record | **537 / 537** |
| `rec+0x2C` lands inside `[0, 0x30 + [file+0x10])` | **537 / 537** |

The second is the sharper one: it is the loader's own guard bit, and finding it
set on disk would have meant I had the field wrong. The third ties the record
array and the sub-arrays into the single region `[+0x10]` measures, which cycle
1196 had established only as a boundary.

## Correcting a number from this cycle's own working

Before reading `0x823555D0` I measured `[file+0x10] / [file+0x0A]` and got 280
(`0x118`) exactly for 345 files, 288 (`0x120`) for 102, and a remainder for 90 —
and started treating `0x118` as a probable stride. **That was an average, not a
stride.** The records are `0x30`; the rest of the region is the sub-arrays they
point into, which vary in length per record. A quotient that comes out exact for
two thirds of a corpus is precisely the kind of near-fit that cycles 1111 and
1113 were killed for, and it survived about four minutes.

## Not established, stated plainly

- `0x82355468`, the sub-record visitor. It is the next call and it is unread.
- What `rec+0x20` points at. It is relocated by `[obj+0x90]` — the *end* boundary
  from cycle 1196, so it indexes the section body rather than the record region —
  but nothing here says what it addresses.
- The `0x30`-byte record's other fields. Four are read (`+0x20`, `+0x26`, `+0x2A`,
  `+0x2C`); the remaining forty-odd bytes are untouched by this function.
- `0x82362190`, called once at the top of `0x823556E0` before anything is read.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
537/537 on all three predictions above
```

No product code changed.
