# Cycle 1263 — a cache shaped like the stride formula, and the shape is all there is

## Qualification

`ghidra-projects-xenon/ac6-xenon`; data read directly from
`analysis-input/ACE6_X360.exe`; `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## Established — `0x82345098` clears two 0x480-byte tables in a 3 x 6 x 8 nest

```
82345098  or     r11,r3,r3
8234509c  li     r8,0x3
823450a0  li     r7,0x0
823450a4  li     r9,0x6
823450a8  li     r10,0x8
823450ac  stw    r7,0x0(r11)      ; zero one word
823450b0  subic. r10,r10,0x1
823450b4  addi   r11,r11,0x8      ; advance EIGHT bytes
823450b8  bne    0x823450ac
823450bc  subic. r9,r9,0x1
823450c0  bne    0x823450a8
823450c4  subic. r8,r8,0x1
823450c8  bne    0x823450a4
823450cc  addi   r11,r3,0x480     ; the second table
```

Three nested counters — **3, then 6, then 8** — writing one zero word per
8-byte slot. That is 3 × 6 × 8 = 144 slots of 8 bytes = **0x480 bytes**, and the
second block begins at exactly `r3 + 0x480`. Two tables, cleared identically,
and `0x82345100` — the stride function, the NDXR chain's last stage — begins
four instructions later.

Those three bounds have the same arity as the stride formula's index spaces.
The rule established earlier is

```
stride = T8[hi & 0xF] + T18[((lo >> 4) - 1) * 6 + (lo & 0xF)]
```

with `T8` holding **8** entries at `0x820110F0` and `T18` holding **18** at
`0x82011130` — and 18 is laid out as `(lo>>4)-1` ∈ [0, 3) by `(lo&0xF)` ∈ [0, 6),
which is the `3` and the `6`. So a `[3][6][8]` table would hold one slot per
`(hi, lo)` pair the formula can be asked about.

I wrote, at this point in the draft, that this is *"structural corroboration of
the stride rule from a function that does not compute it"*. **Then I tested the
adjacency it rests on, and it failed.**

Callers, enumerated by decoding every `bl` in the flat image:

- `bl 0x82345098` — **one** caller, `0x8233B318`.
- `bl 0x82345100` — **one** caller, `0x8233E6E4`, which is inside `0x8233E6B0`,
  the container initialiser cycle 1262 read.
- Neither address appears as aligned data, so neither is reached through a
  vtable or a dispatch table.

  > **Corrected by cycle 1267.** `0x82345100` **does** appear once as aligned
  > data, at `0x82085D20` — its own `.pdata` row. `0x82345098` has no row and no
  > hit at all. The conclusion survives, because an exception record is not a
  > vtable, but the observation as written is wrong, and it is the same error
  > cycle 1266 caught in cycle 1265 — present here one cycle earlier and
  > unnoticed.

The two functions are **four instructions apart in the image and have entirely
separate single callers.** Nothing ties the cleared tables to the stride path
except that the code sits next to it, which is what "adjacency" means and not
what evidence means.

So the honest statement is narrower: **the loop nest is 3 × 6 × 8, which matches
the arity of the stride formula's index spaces** — `T8` has 8 entries, `T18` has
18 laid out as 3 by 6 — and that match is **suggestive and uncontrolled**. 144
eight-byte slots could be dimensioned for something else entirely, and one
caller in a different function is exactly the evidence one would want against
the reading, not for it.

What the enumeration did establish is unrelated and worth keeping:
**`0x8233E6B0` calls the stride builder**, confirmed by a call edge rather than
by reading its disassembly, which is what cycle 1262 asserted from the listing
alone.

## Established — the other two are data, and they read as expected

- **`0x820111C0`** is inside the element declaration lists, at a terminator:
  `00ff0000 ffffffff`. The `0x00FF` is `D3DDECL_END`'s stream field and the
  `0xFFFFFFFF` the padding that follows — consistent with cycle 1233 citing
  `0x820111D8` and `0x820111FC` as list entries, and with cycle 1262's
  independent 12-byte, `0xFF`-terminated walk at `0x821DE898`.
- **`0x82068278`** is the `Type` dword decoder's size table, a byte array whose
  dump (`00000000 00000101 …`) matches the established mapping
  {6:1, 7:1, 16:1, 17:1, 25:1, 26:2, …} in dwords. Nothing new; it is confirmed
  to be data rather than code, which is what the enumeration needed.

## What this settles for the v4 contract

Eight of the nine remaining `ndxr_container` addresses are now read or confirmed
as data. The ninth is read but **not attributable**, because the thing that
would attribute it — its single caller — has not been. The split decision cycle
1261 deferred can be made on readings instead of plausibility for eight:

| address | what it is | where it belongs |
|---|---|---|
| `0x821DE790`, `0x821DE898`, `0x821FC070` | declaration allocate/bind, resource-type dispatch | the D3D device / geometry path |
| `0x8233E6B0` | container initialiser, and it calls the stride builder (call edge) | the container |
| `0x82345098` | clears two 0x480 tables in a 3 x 6 x 8 nest; single caller `0x8233B318`, **unread** | **undecided** |
| `0x820111C0` | an element-list terminator | the geometry path |
| `0x821EC598`, `0x82068278` | the `Type` decoder and its size table | texture / format decoding |
| `0x8233EBB0`, `0x8234AE00` | registry find; registry `+0x80` hop | a registry behaviour that does not exist |

**The contract is still not edited.** Knowing where eight addresses belong is not
the same as having implemented them, and JV is red for the second reason, not
the first. What this cycle removes is the excuse that the split cannot be
designed without more reading.

## Not established

- **Whether the two `0x480` tables are one cache or two.** They are cleared
  identically and adjacently; nothing here reads them.
- **What an 8-byte slot holds.** Only its first word is zeroed, so the other
  four bytes are either unused or initialised elsewhere.
- **What `0x8233B318` is**, and therefore what the two tables are for. That is
  now the question, and it is a different one from the one this cycle opened
  with.
- Whether the `3 × 6 × 8` shape means anything at all. It matches the stride
  formula's arity and nothing else supports the connection.
