# Cycle 1281 — `galib::CGaObj` is a label we attached

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; `analysis/class-map.tsv` (811
vtables, RTTI-derived, J2-gated). `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## The question cycle 1280 left

`0x820078D0` is called **`galib::CGaObj`** throughout the reports and in
`INSTRUMENT_DISCIPLINE.md`. Cycle 1280 found it carries **`0x00000000` at
`vtable-4`** and is absent from the audited class map, and asked where the name
came from.

## Established — the class exists, and this is not its vtable

`.?AVCGaObj@galib@@` is a real type descriptor in the image, and the class map
resolves it:

```
0x820572c0   galib::CGaObj   .?AVCGaObj@galib@@@0   .?AVCGaObj@galib@@
```

Verified here: `0x820572C0 − 4` holds `0x8206EBFC`, whose `+0x0C` reaches a
descriptor naming `.?AVCGaObj@galib@@`. **`galib::CGaObj`'s vtable is
`0x820572C0`.** `0x820078D0` is a different table with no locator at all.

### And they are not near-relatives

My first comparison printed six slots, saw `820adcf0 820add08 82226cd0 82126200
82226cd0` shared, and I wrote that the two tables are "identical from `+0x04`
onwards" — a derived class overriding slot 0.

**Over 96 slots: 11 identical, 85 different.**

```
slot +0x00   CGaObj 0x820ADD20   vs  0x820078D0 0x822A8818
slot +0x24   CGaObj 0x82211538   vs  0x820078D0 0x8229C920
slot +0x2C   CGaObj 0x822663A8   vs  0x820078D0 0x82299E68
slot +0x38   CGaObj 0x8206ECE0   vs  0x820078D0 0x8229C0E0
```

Six samples read as a pattern, and the pattern was six samples long. That is
*an instrument calibrated on one specimen* with the specimen being a prefix —
in the middle of a cycle written to correct a naming error.

Note slot `+0x24`: on `0x820078D0` it is `0x8229C920`, the order handler cycle
1275 traced. On the real `CGaObj` it is `0x82211538`. **The order handler is not
a `CGaObj` slot.**

## What this changes, and what it does not

**The field facts stand.** `+0x184`, `+0x170`, `+0x118` and `+0x188` were read
out of `0x820A7070`'s instructions, and the placement chain was read out of
`0x8229AE7C` and `0x822A23D8`. None of that came from the name.

**The label does not.** "The loop's `r31` is a `galib::CGaObj`" is unsupported:
the vtable is an unnamed class sharing 11 of 96 slots with `CGaObj`, which is
consistent with a common base several levels up and not with being it.

Every sentence of the form *"on the `CGaObj`, field `+0x184`"* should be read as
*"on the object whose vtable is `0x820078D0`"*, which is what was actually
measured. The two families remain **distinct and correctly distinguished** —
that was never the class map's doing, it came from `subi r3,r10,0xf0` versus
`subi r3,r10,0x268` and from the field offsets themselves.

## Not established

- **What `0x820078D0`'s class is.** No locator, not in the map, and no string
  found for it. The eleven shared slots give a common ancestor, not a name.
- **Where the label came from originally.** A prior session, plausibly by the
  same prefix comparison this cycle just made and corrected.
- Whether the same mislabelling affects `0x82009440`, called "the unit family".
  It also has no locator — `vtable-4` holds a function address — and it was not
  compared against any named table here.
