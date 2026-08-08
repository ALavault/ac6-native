# Cycle 1217 — the vertex stride is derived, and cycle 1198 had half of it

## The last file-external gap is closed

`0x828711F0` is not a global. It is **field `+0x170` of the renderer object at
`0x82871080`**, and no instruction in the image materialises its address — every
site spells it `addi rX, rY, 0x170` on a register already holding the object.
That is the aliasing trap of cycle 1212 for the third time, and it is why the
writer was never found.

```
823d36f8  static init:  r3 = 0x82871080 ; bl 0x8233B2E8
8233b314    addi r3,r30,0x170 ; bl 0x82345098   ; zero 144 entries x 8 bytes, twice
8234f8a0  device creation: bl 0x821E6570 (HRESULT, bge)
8234f8c0    bl 0x8234F7C0 -> 0x8233E6B0
8233e6e0      addi r3,r30,0x170
8233e6e4      bl 0x82345100                     ; <- THE WRITER, one caller
8233e704      stw r31,0x4(r30)                  ; publishes the device
```

The writer runs **eight instructions before** the store that publishes
`[0x82871084]` — the very word the draw at `0x82364518` loads (`8236453c lwz
r31,0x4(r25)`) and hands to `SetVertexDeclaration`. **The table cannot be unbuilt
on any path where the draw has a device.** The zeroer clears only the declaration
word, never the stride, so the builder is mandatory rather than optional.

## The rule

Two const `.rdata` tables of 8-byte records `{u16 stride; u16 count; const elems*}`,
read here rather than taken on report:

```
T8  @ 0x820110F0, 8 records  : [16, 32, 48, 64, 16, 24, 20, 36]
T18 @ 0x82011130, 18 records : [4, 8, 8, 12, 12, 16, 8, 16, 12, 20, 16, 24, 4, 8, 8, 12, 12, 16]
```

indexed by `hi & 0xF` and `((lo>>4)-1)*6 + (lo&0xF)`, where `hi` = `desc+0x0E`
and `lo` = `desc+0x0F`. The builder concatenates the two element lists, offsetting
the second by `T8[i].stride`, and stores

```
823451d4  lhz r10,-0x4(r29)   ; T18[j].stride
823451e0  add r10,r10,r7      ; + T8[i].stride
823451e4  stw r10,0x4(r31)
```

**`stride = T8[i] + T18[j]`.** Computed here:

| code | `T8[i]` | `T18[j]` | sum | the product carries |
|---|---|---|---|---|
| `0x0611` | 20 | 8 | **28** | 28 |
| `0x0613` | 20 | 12 | **32** | 32 |
| `0x0711` | 36 | 8 | **44** | 44 |
| `0x0721` | 36 | 16 | **52** | 52 |

**Four of four**, from a formula that was not fitted to them.

## Cycle 1198 was not wrong; it was half-read

Cycle 1198 recorded `0x82012C40[6] = 0x14 = 20` and called it the vertex stride,
"derived but uncontrolled". Cycle 1212 then called it *not a vertex stride at
all*. Both were partly wrong.

`0x82012C40`'s eight dwords are `10 20 30 40 10 18 14 24` — **the same eight
values as `T8`'s strides**, in a different layout. So `20` is `T8[6]`: the **first
term of the sum**, a real quantity with a real meaning, and short by `T18[j]`.
The uncontrolled number was not noise; it was an addend read as a total.

That is a third distinct failure shape in this one constant — uncontrolled
(1198), dismissed (1212), and now resolved — and all three readings were of the
same eight bytes.

## Controls, and the rivals are built from the same tables

The corpus test over 179 distinct files and 4,338 descriptors, extent
`max(desc+0x04 + count·stride) == [buf+0x18]`:

| hypothesis | `% stride == 0` | extent exact |
|---|---|---|
| **derived `T8[i] + T18[j]`** | **4338/4338** | **178/179** |
| rival: `T18[j]` alone | 1586/4338 | **0/179** |
| rival: `T8[i]` alone | 4083/4338 | **0/179** |
| rival: byte roles swapped | 377/4338 | **0/179** |
| rival: constant 16 | 4334/4338 | **0/179** |

The two strongest rivals are the derived formula **with one term dropped**, and
both die at zero. The single inexact file is short by 8 bytes, matching cycle
1212's note.

And an internal control: the builder's second pass fills a *second* table with
`{decl*, u16 T8[i], u16 T18[j]}` separately, and its reader issues two
`SetStreamSource` calls with those two halves. **A separately-coded loop stores
exactly the two terms whose sum the first table stores.**

## A false negative caught in flight

The first search was for a PC-shaped 8-byte `D3DVERTEXELEMENT9` terminator: **zero
hits image-wide, and wrong.** The Xbox 360 element is **12 bytes**, proven by
`0x821DE898`'s own walk (`addi r10,r10,0xc`, `mulli r11,r11,0xc`) and by the
12-byte terminator at `0x820111C0`. Believing that zero would have closed the
line — the exact failure `INSTRUMENT_DISCIPLINE.md` opens with.

## Not established, stated plainly

- The `D3DDECLTYPE` semantics. Element *sizes* follow from the offsets and the
  record strides; the format names do not, and `0x1A23A6` (Stream 3) is
  undetermined.
- `0x0711` and `0x0721` have **no far-side control**: the corpus holds only
  `0x0613` (4,326) and `0x0611` (12).
- Whether Mission 01 reaches the second table's draw path (`0x82345450` from
  `0x823649D0`).
- Writers of `[0x82871084]` are not exhaustively enumerated; a write through
  another alias is not excluded.

## What this unblocks

`native_geometry_raster.cpp`'s stride map — measured and unaudited since before
this session, flagged by cycle 1189, contested by cycle 1203 — is now **derived
end to end**, with every constant readable from the image and a formula that
reproduces all four. JV 2g's last file-external unknown is gone.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
T8 and T18 read here; 4/4 codes reproduced; extent control 178/179 against 0/179 four ways
```

No product code changed in this cycle.
