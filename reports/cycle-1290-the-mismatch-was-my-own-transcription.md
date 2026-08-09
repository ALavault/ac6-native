# Cycle 1290 — the mismatch was my own transcription

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; instructions re-read in
`ghidra-projects-xenon/ac6-xenon`. `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## The thing I told the next reader to check first

Cycle 1289 ended by flagging a contradiction and calling it *"the first thing to
check before anyone treats the record layout as understood"*: the data puts
`7.0` (view 0) or `0.35` (views 1 and 2) at `record+0x60/+0x64`, while — I
wrote — the fallback initialiser puts **46°** at the manager offsets those
fields land on.

**There is no contradiction. The initialiser does not put 46° there.**

## Established — the loop, read properly

```
8225ce20  lfs  f13,0x2fd4(r9)      ; [0x82002FD4] = 0.1
8225ce28  addi r11,r3,0x378
8225ce30  lfs  f12,0x7f64(r8)      ; [0x82007F64] = 0.802851 = 46 deg
8225ce34  subi r10,r10,0x1         ; loop top, two iterations, r11 += 4
8225ce38  stfs f13,-0x8(r11)       ; -> +0x370 then +0x374   = 0.1
8225ce3c  stfs f12,0x0(r11)        ; -> +0x378 then +0x37C   = 46 deg
8225ce40  stfs f11,0x8(r11)        ; -> +0x380, +0x384
8225ce48  stfs f13,0x10(r11)       ; -> +0x388, +0x38C       = 0.1
8225ce4c  stfs f0,0x1c(r11)        ; -> +0x394, +0x398
8225ce54  bne  cr6,0x8225ce34
```

The `-0x8(r11)` store uses **`f13`**, which `8225ce20` loaded with `0.1`. The
46° in `f12` goes only to `+0x378/+0x37C` — **exactly where the data puts its
two FOVs**.

So the fallback writes a non-angle at `+0x370/+0x374` and the data writes a
non-angle there too. The record layout and the fallback agree everywhere, which
is what cycle 1289's other four controls already suggested and what this one
now stops contradicting.

## Where it came from

Cycle 1283's report, from a delegated reading, listed
`[0x82007F64] = 0.8028514385 rad → the default of BOTH +0x370/+0x374 and
+0x378/+0x37C`. That is one register wrong. I carried it into cycle 1289 without
reading the loop, built an open question on it, and told the next reader it was
the first thing to check.

They would have found this. The cost was small and the shape is not: **a
mismatch is a claim, and it needs the same reading as an agreement.** I flagged
it as unresolved rather than asserting it, which is why this took one cycle
instead of becoming a hunt for a discrepancy that was never in the image.

## Corrections

- **Cycle 1283**, by name: `+0x370/+0x374` is initialised to `[0x82002FD4] = 0.1`,
  not to 46°. Only `+0x378/+0x37C` gets `[0x82007F64]`.
- **Cycle 1289**, mine: the "unresolved mismatch" in its *Not established*
  section is withdrawn. The remaining items there stand — the group index, array
  A, and the meaning of most record fields, which is still open in the ordinary
  way rather than because two sources disagree.

## Not established

- What `record+0x60/+0x64` *is*. Both readings now agree it is not an angle;
  neither says what it is.
- `+0x36C` receives `[0x82001350] = 2.0` at `8225ce2c`, and `+0x368` the ease
  rate. Neither was traced to a consumer.
