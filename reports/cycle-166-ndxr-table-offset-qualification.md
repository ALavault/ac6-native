# Cycle 166 — qualification of the candidate table words near the NDXR address-point

Date: 2026-07-18 (Europe/Paris)

## Scope and provenance

This is a read-only follow-up on the canonical PAL `default.xex` target:

- target ID: `ac6-xbox360-pal`;
- module: `default.xex`;
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- Ghidra project: `ace-combat-6`;
- image base: `0x82000000`;
- processor: `PowerPC:BE:64:A2ALT-32addr`.

The historical `ace-combat-6-corrected` project was not consulted for this
qualification.

## Exact data layout

The constructor evidence in cycles 141–152 remains important: `0x8205c980`
is the beginning of a candidate table containing several address-points, while
`0x8205c9a4` is the address-point written by the relevant destructor/base path.
Consequently, offsets must be stated relative to the selected address-point;
the table beginning and the address-point are not interchangeable.

The canonical big-endian word dump gives:

```text
0x8205c980 + 0x108 = 0x8205ca88 -> 0x821033a8
0x8205c980 + 0x164 = 0x8205cae4 -> 0x82102e70
0x8205c980 + 0x16c = 0x8205caec -> 0x82106580
```

The region after `0x8205caec` begins with NUL-terminated `.tree.*` strings,
so these three words are the final code-pointer candidates in this contiguous
table region.  Relative to the `0x8205c9a4` address-point they are at
`+0xe4`, `+0x140` and `+0x148`; they are not the previously confirmed worker
slot `+0x5c` (`0x8205ca00 -> 0x82100600`).

## Interpretation

The words at `0x8205ca88` and `0x8205cae4` are therefore stronger than isolated
literal matches: each is an aligned code pointer in the same candidate table
region, and each points to a function bounded by the canonical `.pdata` map.
They support a `cross-match` relationship between the `0x82102e70`/
`0x821033a8` function family and the table region.

They do **not** yet prove that:

- `0x82102e70` or `0x821033a8` is a method of the NDXR sub-object;
- either word is a slot of the `0x8205c9a4` address-point rather than another
  address-point/view in the same table;
- the functions have a stable C++ class or gameplay name;
- the direct reader calls are reached through those table entries at runtime.

The direct call-site evidence from cycle 165 remains independently confirmed:
`0x82103010` and `0x82103228` call `0x82102148` inside the complete body
`0x82102e70..0x821033a7`, with receiver `r19` and boolean return checks.

## Evidence commands

The following headless scripts were run against the canonical project with
`-readOnly -noanalysis`:

```text
ReferencesTo.java 0x82102e70
ReferencesTo.java 0x821033a8
ReferencesTo.java 0x82102148
DumpU32Range.java 0x8205c940 0x8205cb20
DumpBytes.java 0x8205c940 0x2e0
```

`ReferencesTo` reports the exact data references
`0x8205cae4 -> 0x82102e70` and `0x8205ca88 -> 0x821033a8`; the byte/word dump
also shows the contiguous table and its transition to `.tree.*` strings.
Ghidra's FunctionManager still lacks complete function bodies for several
PPC ABI fragments, so the `.pdata` ranges and XenonRecomp output remain the
boundary evidence.

## Status and next step

- `KEEP`: retain the two direct call-sites and the reader's field contract.
- `KEEP_WITH_CLARIFICATION`: retain the candidate table relationship, but
  qualify it by table base (`0x8205c980`) and address-point (`0x8205c9a4`).
- `unknown`: class name, concrete runtime address-point and lifetime of the
  output records.

The next static pass should inspect the callers' stack-output lifetimes and
the indirect dispatches around `0x821033a8`/`0x82106580`. No GUI, Xenia,
controller, VNC or human intervention is needed.
