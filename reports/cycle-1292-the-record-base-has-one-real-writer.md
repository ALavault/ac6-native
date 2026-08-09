# Cycle 1292 — the record base has one real writer

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; instructions re-read in
`ghidra-projects-xenon/ac6-xenon`. `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## Established

Cycle 1291 redirected task #17: the 116-byte script record is a runtime
structure, so the question is what builds the array at `*(this+0x3A4)`.

D-form stores at that displacement, over the whole image, excluding stack and
absolute bases: **18 at `+0x3A4`, 6 at `+0x3AC`**. Of the integer ones, three
matter and two of them are not assignments:

- **`0x8225979C`** — inside a constructor. It stores `r30` into a long run,
  `+0x37C` through `+0x3C4`, with pointers at `+0x378`, `+0x38C` and `+0x3A0`
  that look like embedded sub-object vptrs. **Zeroing**, the same shape as
  `0x822A2330` zeroing the child count.
- **`0x8225A5D4`** — stores one register into `+0x384`, `+0x388`, `+0x398`,
  `+0x39C`, `+0x3AC`, `+0x3B0`, `+0x3C0` in a row. **A reset**, not a load.
- **`0x820962A8`** — the only one that assigns:

```
820962a0  lwz r10,0x0(r28)
820962a4  stw r7,0x3a8(r31)
820962a8  stw r10,0x3a4(r31)     ; the record array base
```

It takes the base from `[r28]`, alongside a count-or-end at `+0x3A8`.

## Not established, and the second item is a real hole

- **What function `0x820962A8` belongs to, and whether it is on the campaign
  path.** It sits in the `0x82096xxx` region; `0x82097560` is the Online loader
  (cycle 1254). Adjacency is not membership — that is the twenty-third shape —
  and no `.pdata` row was consulted for it here.
- **The scan misses store forms.** It covers D-form `stw/stb/sth/stfs/stwu` at a
  literal displacement. It does **not** cover `stwx`, nor a base precomputed as
  `addi rD,rUnit,0x3a0` followed by `stw rS,4(rD)`. The delegated work on
  `[unit+0xDC]` hit exactly that gap and had to add both forms before its "one
  site" meant anything. **So "one real writer" is one real writer of the form
  scanned**, and the stronger claim is not made.

## What it changes for task #17

Step 3 has a starting address instead of a question. The next move is to
identify `0x820962A8`'s function, check it against the campaign loader
`0x8219F8C0` rather than the online one, and — before any negative — re-run the
store scan with the `stwx` and precomputed-base forms the earlier investigation
needed.
