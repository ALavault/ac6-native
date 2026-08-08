# Cycle 1198 — the stream descriptor, four unanimous controls, and one number I cannot control

## The sub-record visitor

`0x82355468(sub, obj+0x10, fileBase)` completes the NDXR chain:

```
82355480  lhz r11,0x22(r31)             ; flags = sub+0x22
82355484  rlwinm. r11,r11,0x0,0x0,0x10  ; already relocated? -> skip

8235548c  addi r30,r31,0x10
82355490  li  r9,0x4                    ; FOUR pointers, at +0x10 +0x14 +0x18 +0x1C
82355498  lwz r10,0x0(r11)
823554a4  add r10,r10,r27               ;   += fileBase, if non-zero
823554a8  stw r10,0x0(r11)

823554b8  lbz r10,0xe(r31)              ; format code = sub+0x0E
823554cc  lhz r8,0xc(r31)               ; element count = sub+0x0C
823554d0  rlwinm r10,r10,0x2,0x1a,0x1d  ; (code & 0xF) * 4
823554c8  addi r11,r11,0x2c40           ; table at 0x82012C40
823554d8  lwzx r11,r10,r11              ; stride = table[code & 0xF]
823554e0  mullw r11,r11,r8
823554e4  stw r11,0x28(r31)             ; sub+0x28 = stride * count

823554fc  bl 0x82355318                 ; per non-null pointer
8235550c  lwz r11,0x88(r28)
82355514  stw r11,0x1c(r31)             ; sub+0x1C = a sequential index
82355520  stw r11,0x88(r28)             ; [obj+0x98] += 1
82355510  addi r3,r31,0x30              ; the next sub-record is sub + 0x30
```

Sub-records are a fixed `0x30` like their parents, they carry **four stream
pointers relocated by the file base**, and each is stamped with a running index
from `[obj+0x98]`.

**That closes cycle 1196's open question.** `this+0x98` — zeroed by both
constructors, read only on a branch cycle 1196 proved dead — is the **total
sub-record count**, and the dead branch would have allocated eight bytes per
sub-record against it.

## Four controls, all unanimous over 13,014 sub-records

| prediction | result |
|---|---|
| `sub+0x22` bit `0x8000` clear on disk | **13,014 / 13,014** |
| all four stream pointers in bounds | **13,014 / 13,014** |
| `sub+0x28` is **zero** on disk | **13,014 / 13,014** |
| `sub+0x0E` high nibble zero | **13,014 / 13,014** |

The third is the good one. The derivation says `+0x28` is *computed at load*,
`stride * count`; if it were a stored field the disk would carry a value. It
carries zero, every time. That is a prediction that could have failed cleanly.

`sub+0x0E` is **6 in all 13,014 records**, so `table[6]` is the only entry this
content uses.

## The number I cannot control, stated as such

`0x82012C40` reads `10 20 30 40 10 18 14 24` — so `table[6] = 0x14`, twenty
bytes, and every stream in Mission 01 is a twenty-byte vertex.

**I have no file-side control that discriminates that value**, and I looked for
two:

- *In bounds.* `stream0 + stride * count <= filesize` holds for **13,014 of
  13,014 at strides `0x10`, `0x14`, `0x18` and `0x20` alike**. Four rivals, all
  perfect. The test cannot fail and therefore proves nothing — the same shape as
  cycle 1196's `+0x30`, where a second test did discriminate.
- *Packing.* If streams were laid consecutively, the gap between adjacent
  `stream0` pointers would be `stride * count`. It is a non-integer multiple for
  **12,228 of 12,477** pairs. That refutes the *packing model*, not the stride —
  the four streams interleave, so sorting one of them mixes it with the others.

So `0x14` rests on the instruction path alone: the byte is 6, the mask is four
bits, the table entry is `0x14`. That is a derivation and it is the standard this
repository asks for. But it is **not** corroborated from the files, and cycle
1196 established in this same structure that in-bounds arithmetic will happily
confirm a wrong constant. I am recording the stride as derived-but-uncontrolled
rather than promoting it.

## An unguarded read, worth noting

The mask is `(code & 0xF) * 4` — sixteen entries — but the table has **eight**.
`0x82012C60` onward is unrelated data: `01000004`, then the ASCII of
`XML_DTD` and `sizeof(XML_L`. A file with `sub+0x0E` in `8..15` would multiply a
count by a byte of a string literal. It never happens here, because every record
is 6, and that is content discipline rather than a bounds check.

## Not established, stated plainly

- `0x82355318`, called per non-null stream pointer. It is the last unread call.
- What each of the four streams **is**. Four pointers and one stride do not say
  which is position, which is normal, which is UV.
- The other fields of the `0x30`-byte sub-record. Six are read here.
- Whether the stride `0x14` is per-stream or applies only to stream 0. One size
  is computed and stored once, for four pointers.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
13,014 sub-records across 537 files, four predictions, all unanimous
```

No product code changed.
