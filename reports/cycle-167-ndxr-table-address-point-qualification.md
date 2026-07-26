# Cycle 167 — table boundary and independent address-point at `0x8205caec`

Date: 2026-07-18 (Europe/Paris)

## Target identity

All observations below use the canonical AC6 PAL target:

- target ID: `ac6-xbox360-pal`;
- module: `default.xex`;
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- Ghidra project: `ace-combat-6`;
- image base: `0x82000000`;
- language: `PowerPC:BE:64:A2ALT-32addr`.

No corrected-project data, GUI state, emulator run or human input was used.

## `.pdata` boundaries

The canonical `.pdata` bytes at `0x8207c210` are:

```text
0x82105aa8 -> 0x40003f06
0x82105ba8 -> 0x4001ec05
0x82106358 -> 0x40008a02
0x82106580 -> 0x40001705
0x821065e0 -> 0x40002003
```

Thus the body beginning at `0x82106580` is bounded by the next entry at
`0x821065e0`, i.e. `0x82106580..0x821065df`.  The short Ghidra function
fragments are not used as a substitute for these PPC unwind boundaries.

## Constructor/address-point evidence

The body starts:

```text
0x82106580  mfspr r12,LR
0x82106584  stw   r12,-0x8(r1)
0x82106588  std   r30,-0x18(r1)
0x8210658c  std   r31,-0x10(r1)
0x82106590  stwu  r1,-0x70(r1)
0x82106594  lis   r11,-0x7dfa
0x82106598  or    r31,r3,r3
0x8210659c  subi  r11,r11,0x3514
0x821065a0  or    r30,r4,r4
0x821065a4  stw   r11,0x0(r31)
0x821065a8  bl    0x821065e0
```

On Xenon, `lis r11,-0x7dfa` followed by `subi r11,r11,0x3514` constructs
`0x8205caec`.  The function then stores that value at `receiver+0`.
Therefore `0x8205caec` is an independently observed vtable/address-point
value installed by the constructor-like body `0x82106580`; it is not merely an
uninterpreted function-pointer literal.

The data word at that address is itself:

```text
0x8205caec -> 0x82106580
```

This is consistent with a table address-point whose first virtual entry is the
constructor/destructor-family body, but it does not establish the C++ class
name or the relationship to the NDXR sub-object at `0x8205c980`.

## Separation from the reader-table candidates

The nearby words remain:

```text
0x8205ca88 -> 0x821033a8
0x8205cae4 -> 0x82102e70
0x8205caec -> 0x82106580   # independently qualified address-point
```

The earlier cycle-166 interpretation is refined accordingly: `0x82102e70` and
`0x821033a8` remain `cross-match` candidates in the surrounding table, while
`0x8205caec` is now a `confirmed` address-point value for the body that writes
it into a receiver.  This does not promote either neighboring function to a
confirmed NDXR method.

## Adjacent method evidence

The canonical body at `0x821033a8` is bounded by `.pdata` at `0x82103488` and
begins by preserving `r3` as `r31`.  It reads `receiver+0x4088`; in the
non-null branch it calls `0x82104930`, loads a global table, and dispatches
through the selected object's vtable at offset `0x14c`.  In the alternate branch
it reads `receiver+0x4090` and `receiver+0x408c`, calls `0x82103488`, then
performs two indirect calls through vtable offset `0x148`.

These are confirmed instruction-level facts only.  Their owner and semantic
role remain `unknown`; do not merge their `+0x4088/+0x4090` fields with the
NDXR `+0x28/+0x30/+0x5c` fields.

## Evidence commands

Read-only headless scripts used:

```text
FindU32Set.java 0x82106580 0x82102e70 0x821033a8
DumpBytes.java 0x8207c210 64
DumpU32Range.java 0x8205ca80 0x8205cb00
DumpRange.java 0x821033a8 0x82103488
DumpRange.java 0x82106580 0x82106700
```

## Decision

- `KEEP`: direct calls from `0x82102e70` to the reader and the reader field
  contract;
- `KEEP_WITH_CLARIFICATION`: surrounding table relationship, now split by
  the independently proven address-point `0x8205caec`;
- `unknown`: C++ class names, runtime dispatch path and the semantic relation
  between the `0x821033a8` owner fields and NDXR.

The next useful static step is to follow references to the constructed object
whose first word is `0x8205caec`, then compare its lifetime with the table
records passed to the two confirmed reader call-sites. No human intervention is
required.
