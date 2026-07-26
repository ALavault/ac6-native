# Cycle 165 — direct callers of the NDXR reader

Date: 2026-07-18 (Europe/Paris)

## Scope

This pass follows the canonical PAL Xbox 360 target only.  The identity used
for every address below is:

| Field | Value |
|---|---|
| target | `ac6-xbox360-pal` |
| module | `default.xex` |
| SHA-256 | `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` |
| image base | `0x82000000` |
| Ghidra project | `ace-combat-6` (canonical) |
| language | `PowerPC:BE:64:A2ALT-32addr` |

The historical project `ace-combat-6-corrected` is not used for this report.
Its bytes differ for the same nominal target and remain `needs-revalidation`.

## Function-boundary evidence

The canonical `.pdata` records are:

```text
0x82102148 -> metadata 0x40010706, next entry 0x82102568
0x82102e70 -> metadata 0x40014d05, next entry 0x821033a8
0x821033a8 -> metadata 0x40003703, next entry 0x82103488
```

Therefore the ABI function body containing the calls in this pass is
`0x82102e70..0x821033a7`, and the reader body is bounded by
`0x82102148..0x82102567`.  Ghidra's FunctionManager currently exposes short
ABI fragments at these entries (two instructions for `0x82102e70`), so those
fragments must not be mistaken for the complete PPC functions.  The raw
`.pdata` records and XenonRecomp mapping are the stronger boundary evidence.

## Confirmed direct call-sites

Raw PPC disassembly from the canonical project shows two direct branches from
the `0x82102e70` body to `0x82102148`.

### Call-site `0x82103010`

```text
0x82103000  addi   r6,r1,0x60
0x82103004  addi   r5,r1,0x90
0x82103008  addi   r4,r1,0x80
0x8210300c  or     r3,r19,r19
0x82103010  bl     0x82102148
0x82103014  rlwinm r11,r3,0,24,31
0x82103018  cmplwi cr6,r11,0
0x8210301c  beq    cr6,0x82103044
```

This is a confirmed direct call.  The receiver is copied from `r19` to `r3`;
three stack/output pointers are passed in `r4`, `r5` and `r6`; and the return
value is narrowed to an 8-bit boolean before the conditional branch.

### Call-site `0x82103228`

```text
0x82103210  addi   r8,r1,0x70
0x82103214  addi   r7,r1,0xa0
0x82103218  addi   r6,r1,0x60
0x8210321c  addi   r5,r1,0x90
0x82103220  addi   r4,r1,0x80
0x82103224  or     r3,r19,r19
0x82103228  bl     0x82102148
0x8210322c  rlwinm r11,r3,0,24,31
0x82103230  cmplwi cr6,r11,0
0x82103234  beq    cr6,0x8210327c
```

This is a second confirmed direct call in the same function.  It uses the same
receiver and the first three output pointers, plus two additional output
locations in `r7` and `r8`, and applies the same boolean-return check.

XenonRecomp's `sub_82102e70` output independently preserves `r19` as the
receiver and emits both calls at these addresses.  This cross-check separates
the call-site fact from Ghidra's incomplete xref/function model.

## Table/reference observations

Canonical read-only data contains these little-endian words:

```text
0x8205cae4 -> 0x82102e70
0x8205ca88 -> 0x821033a8
0x8207c1c8 -> 0x82102148   (.pdata)
0x8207c1d0 -> 0x82102568   (.pdata)
```

The first two are candidates for table/vtable references, supported by their
placement near `0x8205c9a4`, but they do not yet prove a C++ class, slot, or
semantic owner.  They are therefore `cross-match`, not `confirmed` vtable
identity.

## Qualification

- The reader's loads from the receiver's `+0x28`, `+0x30`, `+0x5c`, `+0x74`
  and `+0x78` remain a `KEEP`/confirmed static observation on the canonical
  bytes and XenonRecomp body.
- Its direct use as an NDXR vtable method remains `KEEP_WITH_CLARIFICATION`:
  the receiver/dispatch relationship is plausible and previously cross-matched,
  but no single table word proves the C++ class or slot yet.
- The two direct call-sites are `confirmed` and should be used as the next
  anchor for recovering the reader contract.
- The semantic names of the receiver, output records and the boolean condition
  remain `unknown`; do not rename them as gameplay concepts.

## Commands and evidence sources

The evidence was collected read-only with Ghidra `analyzeHeadless` scripts
`DumpRange.java`, `DumpBytes.java`, `InspectFunctionIsland.java` and
`FindU32Set.java`, then cross-checked against:

```text
.tools/recomp-eval/ac6/output/ppc_recomp.10.cpp
```

No Ghidra project, generated recompiler output, binary, or proprietary asset
was modified.  No Wine, Xenia, VNC, controller, or other human interaction was
required.

## Next bounded step

Inspect the callers' stack-output lifetimes and the candidate table words near
`0x8205ca88/0x8205cae4`, then compare any indirect dispatch with the canonical
`.pdata` and XenonRecomp boundaries.  Keep the semantic conclusion bounded
until a type, vtable slot, or dynamic trace independently confirms it.
